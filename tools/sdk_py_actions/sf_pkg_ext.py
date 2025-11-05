# -*- coding:utf-8 -*-
# SPDX-FileCopyrightText: 2025 SiFli
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import subprocess
import json
import base64
import platform
import socket
import uuid
from typing import Any, Dict, Optional

import click
from click.core import Context
from sdk_py_actions.tools import PropertyDict

from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

sdk_tools_path = os.environ.get('SIFLI_SDK_TOOLS_PATH') or os.path.expanduser(os.path.join('~', '.sifli'))
CREDENTIALS_FILE = os.path.join(sdk_tools_path, '.sf-pkg')

# Encryption key derivation - using a fixed salt for simplicity
# In production, you might want to use a more secure approach
SALT = b'sifli_sdk_salt_v1'


def _machine_identifier() -> bytes:
    """
    Derive a machine-specific identifier.
    Prefers os.uname() when available to keep backward compatibility with previous keys.
    """
    try:
        return os.uname().nodename.encode()
    except AttributeError:
        pass

    candidates = []

    env_host = os.environ.get('COMPUTERNAME') or os.environ.get('HOSTNAME')
    if env_host:
        candidates.append(env_host)

    node = platform.node()
    if node:
        candidates.append(node)

    try:
        hostname = socket.gethostname()
        if hostname:
            candidates.append(hostname)
    except OSError:
        pass

    # uuid.getnode() is stable for a given machine (MAC or random fallback)
    mac = uuid.getnode()
    if mac:
        candidates.append(hex(mac))

    # Deduplicate while preserving order
    seen = set()
    unique_parts = []
    for part in candidates:
        part = str(part).strip()
        if part and part not in seen:
            unique_parts.append(part)
            seen.add(part)

    identifier = '::'.join(unique_parts) if unique_parts else 'sifli-default'
    return identifier.encode()


def get_encryption_key() -> bytes:
    """Generate encryption key from machine-specific data"""
    machine_id = _machine_identifier()

    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=32,
        salt=SALT,
        iterations=100000,
    )
    key = base64.urlsafe_b64encode(kdf.derive(machine_id))
    return key

def encrypt_data(data: str) -> str:
    """Encrypt data using Fernet symmetric encryption"""
    key = get_encryption_key()
    f = Fernet(key)
    encrypted = f.encrypt(data.encode())
    return encrypted.decode()

def decrypt_data(encrypted_data: str) -> str:
    """Decrypt data using Fernet symmetric encryption"""
    try:
        key = get_encryption_key()
        f = Fernet(key)
        decrypted = f.decrypt(encrypted_data.encode())
        return decrypted.decode()
    except Exception as e:
        print(f"Error decrypting data: {e}")
        return ""

def load_credentials() -> Dict[str, str]:
    """Load credentials from encrypted file"""
    if not os.path.exists(CREDENTIALS_FILE):
        return {}
    
    try:
        with open(CREDENTIALS_FILE, 'r') as f:
            encrypted_content = f.read()
            if not encrypted_content:
                return {}
            
            decrypted_content = decrypt_data(encrypted_content)
            if not decrypted_content:
                return {}
            
            return json.loads(decrypted_content)
    except Exception as e:
        print(f"Error loading credentials: {e}")
        return {}

def save_credentials(credentials: Dict[str, str]) -> None:
    """Save credentials to encrypted file"""
    try:
        # Ensure directory exists
        os.makedirs(sdk_tools_path, exist_ok=True)
        
        # Encrypt and save
        json_content = json.dumps(credentials, indent=2)
        encrypted_content = encrypt_data(json_content)
        
        with open(CREDENTIALS_FILE, 'w') as f:
            f.write(encrypted_content)
        
        # Set file permissions to be readable only by owner
        os.chmod(CREDENTIALS_FILE, 0o600)
        
    except Exception as e:
        print(f"Error saving credentials: {e}")

def remove_credentials() -> None:
    """Remove credentials file"""
    try:
        if os.path.exists(CREDENTIALS_FILE):
            os.remove(CREDENTIALS_FILE)
            print(f"Removed credentials file: {CREDENTIALS_FILE}")
    except Exception as e:
        print(f"Error removing credentials file: {e}")

def action_extensions(base_actions: Dict, project_path: str) -> Any:

    def add_dep_callback(target_name: str, ctx: Context, args: PropertyDict) -> None:
        try:
            result = subprocess.run(["conan", "new", "sf-pkg-dep"], check=False)
            if result.returncode == 0:
                print("You can now add dependent packages in conanfile.txt")
            else:
                print("Error: Failed to create dependency file")
        except Exception as e:
            print(f"Error creating dependency file: {e}")
    
    def install_callback(target_name: str, ctx: Context, args: PropertyDict) -> None:
        try:
            subprocess.run([
                "conan", "install", ".", 
                "--output-folder=sf-pkgs", 
                "--deployer=full_deploy", 
                "-r=artifactory"
            ], check=True)
            print("Packages installed successfully")
        except subprocess.CalledProcessError as e:
            print(f"Error installing packages: {e}")
        except Exception as e:
            print(f"Error: {e}")
    
    def new_callback(target_name: str, ctx: Context, args: PropertyDict, name: str = "mypackage") -> None:
        try:
            subprocess.run([
                "conan", "new", "sf-pkg", 
                "-d", f"name={name}"
            ], check=True)
            print(f"Created new package: {name}")
        except subprocess.CalledProcessError as e:
            print(f"Error creating new package: {e}")
        except Exception as e:
            print(f"Error: {e}")
    
    def build_callback(target_name: str, ctx: Context, args: PropertyDict, version: str) -> None:
        try:
            subprocess.run([
                "conan", "create", 
                "--version", version, "."
            ], check=True)
            print(f"Package built successfully with version: {version}")
        except subprocess.CalledProcessError as e:
            print(f"Error building package: {e}")
        except Exception as e:
            print(f"Error: {e}")
    
    def upload_callback(target_name: str, ctx: Context, args: PropertyDict, name: str, keep: bool = False) -> None:
        try:
            # Upload package
            subprocess.run([
                "conan", "upload", name, "-r=artifactory"
            ], check=True)
            print(f"Package {name} uploaded successfully")
            
            # Remove from local cache unless --keep is specified
            if not keep:
                subprocess.run(["conan", "remove", name, "-c"], check=False)
                print(f"Package {name} removed from local cache")
            
            # Sync package to public repository
            try:
                import requests
                
                # Load credentials to get token
                credentials = load_credentials()
                if not credentials:
                    print("Warning: No credentials found. Skipping sync to public repository.")
                    return
                
                # Use the first available token (or you can specify which user to use)
                token = next(iter(credentials.values())) if credentials else None
                
                if not token:
                    print("Warning: No token available. Skipping sync to public repository.")
                    return
                
                # POST request to sync API
                sync_url = "https://packages.sifli.com/api/v1/sync"
                headers = {
                    "Authorization": f"Bearer {token}",
                    "Content-Type": "application/json"
                }
                
                print("Syncing package to public repository...")
                response = requests.post(sync_url, json={}, headers=headers, timeout=30)
                
                if response.status_code == 200:
                    print("Package synced to public repository successfully")
                else:
                    print(f"Warning: Sync failed with status code {response.status_code}")
                    print(f"Response: {response.text}")
                    
            except ImportError:
                print("Warning: requests library not available. Skipping sync to public repository.")
            except requests.exceptions.RequestException as e:
                print(f"Warning: Failed to sync to public repository: {e}")
            except Exception as e:
                print(f"Warning: Unexpected error during sync: {e}")
                
        except subprocess.CalledProcessError as e:
            print(f"Error uploading package: {e}")
        except Exception as e:
            print(f"Error: {e}")
    
    def remove_callback(target_name: str, ctx: Context, args: PropertyDict, name: str) -> None:
        try:
            subprocess.run(["conan", "remove", name, "-c"], check=True)
            print(f"Package {name} removed from local cache")
        except subprocess.CalledProcessError as e:
            print(f"Error removing package: {e}")
        except Exception as e:
            print(f"Error: {e}")

    def login_callback(target_name: str, ctx: Context, args: PropertyDict, name: str, token: str) -> None:
        """Login to SiFli package registry and store credentials"""
        try:
            # First, try to login with conan
            subprocess.run([
                "conan", "remote", "login", "-p", token, "artifactory", name.lower()
            ], check=True)
            print("Logged in to SiFli package registry")
            
            # If login successful, store credentials
            credentials = load_credentials()
            credentials[name] = token
            save_credentials(credentials)
            print(f"Credentials stored for user: {name}")
            
        except subprocess.CalledProcessError as e:
            print(f"Error logging in: {e}")
        except Exception as e:
            print(f"Error: {e}")
    
    def logout_callback(target_name: str, ctx: Context, args: PropertyDict, name: Optional[str] = None) -> None:
        """Logout from SiFli package registry and clear credentials"""
        try:
            # Load existing credentials
            credentials = load_credentials()
            
            if name:
                # Logout specific user
                if name in credentials:
                    # Remove from conan
                    subprocess.run([
                        "conan", "remote", "logout", "artifactory"
                    ], check=False)  # Don't fail if already logged out
                    
                    # Remove from stored credentials
                    del credentials[name]
                    
                    if credentials:
                        # Save remaining credentials
                        save_credentials(credentials)
                    else:
                        # Remove file if no credentials left
                        remove_credentials()
                    
                    print(f"Logged out user: {name}")
                else:
                    print(f"User {name} not found in stored credentials")
            else:
                # Logout all users
                subprocess.run([
                    "conan", "remote", "logout", "artifactory"
                ], check=False)
                
                # Remove all credentials
                remove_credentials()
                print("Logged out all users and cleared credentials")
                
        except subprocess.CalledProcessError as e:
            print(f"Error logging out: {e}")
        except Exception as e:
            print(f"Error: {e}")

    sf_pkg_actions = {
        'actions': {
            'sf-pkg-login': {
                'callback': login_callback,
                'help': 'Login to SiFli package registry and store credentials.',
                'options': [
                    {
                        'names': ['--name', '-n'],
                        'help': 'Username for login.',
                        'required': True,
                    },
                    {
                        'names': ['--token', '-t'],
                        'help': 'API token for login.',
                        'required': True,
                    },
                ],
            },
            'sf-pkg-logout': {
                'callback': logout_callback,
                'help': 'Logout from SiFli package registry and clear credentials.',
                'options': [
                    {
                        'names': ['--name', '-n'],
                        'help': 'Username to logout (if not specified, logout all users).',
                        'required': False,
                        'default': None,
                    },
                ],
            },
            'sf-pkg-add-dep': {
                'callback': add_dep_callback,
                'help': 'Add initial package dependency file.',
            },
            'sf-pkg-install': {
                'callback': install_callback,
                'help': 'Install SiFli-SDK packages.',
            },
            'sf-pkg-new': {
                'callback': new_callback,
                'help': 'Create a new SiFli-SDK package.',
                'options': [
                    {
                        'names': ['--name'],
                        'help': 'Package name.',
                        'default': 'mypackage',
                        'required': True,
                    },
                ],
            },
            'sf-pkg-build': {
                'callback': build_callback,
                'help': 'Build the package for upload.',
                'options': [
                    {
                        'names': ['--version'],
                        'help': 'Version to be built.',
                        'required': True,
                    },
                ],
            },
            'sf-pkg-upload': {
                'callback': upload_callback,
                'help': 'Upload the specified package.',
                'options': [
                    {
                        'names': ['--name'],
                        'help': 'Name of package to be uploaded.',
                        'required': True,
                    },
                    {
                        'names': ['--keep'],
                        'help': 'Keep local cache after upload.',
                        'is_flag': True,
                        'default': False,
                    },
                ],
            },
            'sf-pkg-remove': {
                'callback': remove_callback,
                'help': 'Remove package from local cache.',
                'options': [
                    {
                        'names': ['--name'],
                        'help': 'Name of package to be removed.',
                        'required': True,
                    },
                ],
            },
        }
    }

    return sf_pkg_actions
