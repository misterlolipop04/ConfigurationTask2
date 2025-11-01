import json
import argparse
import sys
import urllib.request
import urllib.error
from typing import Dict, Any, List
from urllib.parse import urlparse


class ConfigError(Exception):
    pass


# Класс для обработки ошибок при получении зависимостей (Этап 2)
class DependencyFetchError(Exception):
    pass


class DependencyVisualizer:
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)
        self._validate_config()
        # Инициализация списка для хранения зависимостей (Этап 2)
        self.dependencies = []

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

        # Валидация URL для npm registry (Этап 2)
        try:
            result = urlparse(self.config['repo_url'])
            if not all([result.scheme, result.netloc]):
                raise ConfigError("repo_url must be a valid URL")

            # Проверка, что URL соответствует формату npm registry (Этап 2)
            if "registry.npmjs.org" not in result.netloc:
                raise ConfigError("repo_url must point to registry.npmjs.org")

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

    # Основной метод для получения зависимостей из npm registry (Этап 2)
    def fetch_dependencies(self):
        """Получает прямые зависимости пакета из npm registry"""
        try:
            print(f"\nFetching dependencies for package: {self.config['package_name']}")

            # Формирование URL для запроса к npm registry API (Этап 2)
            package_url = f"https://registry.npmjs.org/{self.config['package_name']}"

            # Выполнение HTTP запроса к npm registry (Этап 2)
            with urllib.request.urlopen(package_url) as response:
                data = json.loads(response.read().decode())

            # Извлечение информации о последней версии пакета (Этап 2)
            latest_version = data.get('dist-tags', {}).get('latest')
            if not latest_version:
                raise DependencyFetchError("Could not find latest version")

            version_data = data.get('versions', {}).get(latest_version, {})

            # Извлечение прямых зависимостей из данных пакета (Этап 2)
            dependencies = version_data.get('dependencies', {})

            # Сохранение списка зависимостей (Этап 2)
            self.dependencies = list(dependencies.keys())

            print(f"Successfully fetched {len(self.dependencies)} direct dependencies")

        except urllib.error.URLError as e:
            raise DependencyFetchError(f"Network error: {e}")
        except json.JSONDecodeError as e:
            raise DependencyFetchError(f"Invalid JSON response: {e}")
        except Exception as e:
            raise DependencyFetchError(f"Unexpected error: {e}")

    # Вывод всех прямых зависимостей в консоль (Этап 2)
    def print_dependencies(self):
        """Выводит прямые зависимости в консоль"""
        if not self.dependencies:
            print("No direct dependencies found")
            return

        print(f"\nDirect dependencies of '{self.config['package_name']}':")
        for i, dep in enumerate(self.dependencies, 1):
            print(f"  {i}. {dep}")


def main():
    parser = argparse.ArgumentParser(description='Dependency Graph Visualizer for npm packages')
    parser.add_argument('--config', default='config2.json', help='Path to config file (default: config.json)')
    args = parser.parse_args()

    try:
        visualizer = DependencyVisualizer(args.config)
        visualizer.print_config()

        # Этап 2: Вызов методов для получения и вывода зависимостей
        visualizer.fetch_dependencies()
        visualizer.print_dependencies()

        print("\nApplication completed successfully!")

    except ConfigError as e:
        print(f"Configuration error: {e}", file=sys.stderr)
        sys.exit(1)
    except DependencyFetchError as e:
        print(f"Dependency fetch error: {e}", file=sys.stderr)
        sys.exit(2)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(3)


if __name__ == '__main__':
    main()
