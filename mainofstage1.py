import json
import argparse
import sys
from pathlib import Path
from typing import Dict, Any
from urllib.parse import urlparse


class ConfigError(Exception):
    pass


class DependencyVisualizer:
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)
        self._validate_config()

    def _load_config(self, config_path: str) -> Dict[str, Any]:
        try:
            with open(config_path, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            raise ConfigError(f"Config file not found: {config_path}")
        except json.JSONDecodeError as e:
            raise ConfigError(f"Invalid JSON in config file: {e}")

    def _validate_config(self):
        required_fields = [
            'package_name',
            'repo_url',
            'test_repo_mode',
            'output_image',
            'ascii_tree'
        ]

        for field in required_fields:
            if field not in self.config:
                raise ConfigError(f"Missing required field: {field}")

        if not isinstance(self.config['package_name'], str):
            raise ConfigError("package_name must be a string")

        if not isinstance(self.config['repo_url'], str):
            raise ConfigError("repo_url must be a string")

        try:
            result = urlparse(self.config['repo_url'])
            if not all([result.scheme, result.netloc]):
                raise ConfigError("repo_url must be a valid URL")

            if "crates.io" not in result.netloc:
                raise ConfigError("repo_url must point to crates.io")

            if "/crates/" not in result.path:
                raise ConfigError(
                    "repo_url must point to a specific crate (format: https://crates.io/crates/package_name)")

        except Exception as e:
            raise ConfigError(f"Invalid repo_url: {e}")

        if not isinstance(self.config['test_repo_mode'], bool):
            raise ConfigError("test_repo_mode must be a boolean")

        if not isinstance(self.config['output_image'], str):
            raise ConfigError("output_image must be a string")

        if not isinstance(self.config['ascii_tree'], bool):
            raise ConfigError("ascii_tree must be a boolean")

    def print_config(self):
        print("Configuration parameters:")
        for key, value in self.config.items():
            print(f"  {key}: {value}")

    def demonstrate_crates_io_usage(self):
        """Демонстрация работы с конкретным пакетом на crates.io"""
        print(f"\nDemo: Will analyze package '{self.config['package_name']}'")
        print(f"Package URL: {self.config['repo_url']}")

        package_from_url = self.config['repo_url'].split('/')[-1]
        print(f"Package name extracted from URL: {package_from_url}")

        if self.config['test_repo_mode']:
            print("Using test repository mode")
        else:
            print("Using live crates.io registry")


def main():
    parser = argparse.ArgumentParser(description='Dependency Graph Visualizer for Rust/Crates.io')
    parser.add_argument('--config', default='config1.json', help='Path to config file (default: config.json)')
    args = parser.parse_args()

    try:
        visualizer = DependencyVisualizer(args.config)
        visualizer.print_config()
        visualizer.demonstrate_crates_io_usage()

        print("\nApplication initialized successfully!")

    except ConfigError as e:
        print(f"Configuration error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()
