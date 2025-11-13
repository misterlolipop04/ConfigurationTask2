import json
import argparse
import sys
import urllib.request
import urllib.error
from collections import deque
from typing import Dict, Any, List, Set
from urllib.parse import urlparse


class ConfigError(Exception):
    pass


class DependencyFetchError(Exception):
    pass


class CircularDependencyError(Exception):
    pass


class DependencyVisualizer:
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)
        self._validate_config()
        self.dependency_graph = {}
        self.visited = set()
        self.cycles = []

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

        if not isinstance(self.config['test_repo_mode'], bool):
            raise ConfigError("test_repo_mode must be a boolean")

        if not isinstance(self.config['output_image'], str):
            raise ConfigError("output_image must be a string")

        if not isinstance(self.config['ascii_tree'], bool):
            raise ConfigError("ascii_tree must be a boolean")

        # Для тестового режима проверяем наличие test_repo_path
        if self.config['test_repo_mode']:
            if 'test_repo_path' not in self.config:
                raise ConfigError("test_repo_path is required in test mode")
            if not isinstance(self.config['test_repo_path'], str):
                raise ConfigError("test_repo_path must be a string")

    def print_config(self):
        print("Configuration parameters:")
        for key, value in self.config.items():
            print(f"  {key}: {value}")

    def load_test_repository(self):
        """Загружает тестовый репозиторий из файла"""
        try:
            with open(self.config['test_repo_path'], 'r') as f:
                test_data = json.load(f)

            # Валидация тестовых данных - проверяем, что все пакеты используют большие буквы
            for package, dependencies in test_data.items():
                if not package.isupper() or any(not dep.isupper() for dep in dependencies):
                    raise ConfigError("Test repository packages must use uppercase letters only")

            return test_data
        except FileNotFoundError:
            raise DependencyFetchError(f"Test repository file not found: {self.config['test_repo_path']}")
        except json.JSONDecodeError as e:
            raise DependencyFetchError(f"Invalid JSON in test repository: {e}")

    def fetch_npm_dependencies(self, package_name: str) -> List[str]:
        """Получает зависимости пакета из npm registry"""
        try:
            package_url = f"https://registry.npmjs.org/{package_name}"
            with urllib.request.urlopen(package_url) as response:
                data = json.loads(response.read().decode())

            latest_version = data.get('dist-tags', {}).get('latest')
            if not latest_version:
                return []

            version_data = data.get('versions', {}).get(latest_version, {})
            dependencies = version_data.get('dependencies', {})

            return list(dependencies.keys())

        except Exception as e:
            print(f"Warning: Could not fetch dependencies for {package_name}: {e}")
            return []

    def build_dependency_graph_bfs(self):
        """Строит граф зависимостей с помощью BFS"""
        print(f"\nBuilding dependency graph for package: {self.config['package_name']}")

        if self.config['test_repo_mode']:
            # Используем тестовый репозиторий
            test_repo = self.load_test_repository()
            root_package = self.config['package_name']

            if root_package not in test_repo:
                raise DependencyFetchError(f"Package {root_package} not found in test repository")

            # BFS для обхода графа
            queue = deque([root_package])
            visited = set()

            while queue:
                current_package = queue.popleft()
                if current_package in visited:
                    continue

                visited.add(current_package)
                dependencies = test_repo.get(current_package, [])
                self.dependency_graph[current_package] = dependencies

                for dep in dependencies:
                    if dep not in visited:
                        queue.append(dep)
        else:
            # Используем реальный npm registry
            root_package = self.config['package_name']
            queue = deque([root_package])
            visited = set()

            while queue:
                current_package = queue.popleft()
                if current_package in visited:
                    continue

                visited.add(current_package)
                dependencies = self.fetch_npm_dependencies(current_package)
                self.dependency_graph[current_package] = dependencies

                for dep in dependencies:
                    if dep not in visited:
                        queue.append(dep)

    def detect_cycles(self):
        """Обнаруживает циклические зависимости в графе"""
        print("\nDetecting cyclic dependencies...")

        def dfs(node, path):
            if node in path:
                cycle_start = path.index(node)
                cycle = path[cycle_start:] + [node]
                self.cycles.append(cycle)
                return

            if node in visited:
                return

            visited.add(node)

            for neighbor in self.dependency_graph.get(node, []):
                dfs(neighbor, path + [node])

        visited = set()
        for node in self.dependency_graph:
            if node not in visited:
                dfs(node, [])

        if self.cycles:
            print(f"Found {len(self.cycles)} cyclic dependency cycles")
            for i, cycle in enumerate(self.cycles, 1):
                print(f"  Cycle {i}: {' -> '.join(cycle)}")
        else:
            print("No cyclic dependencies found")

    def print_dependency_graph(self):
        """Выводит граф зависимостей"""
        print(f"\nDependency graph for '{self.config['package_name']}':")
        for package, dependencies in self.dependency_graph.items():
            print(f"  {package} -> {dependencies}")


def main():
    parser = argparse.ArgumentParser(description='Dependency Graph Visualizer - Stage 3: Core Operations')
    parser.add_argument('--config', default='config3.json', help='Path to config file (default: config3.json)')
    args = parser.parse_args()

    try:
        visualizer = DependencyVisualizer(args.config)
        visualizer.print_config()

        # Построение графа зависимостей с помощью BFS
        visualizer.build_dependency_graph_bfs()
        visualizer.print_dependency_graph()

        # Обнаружение циклических зависимостей
        visualizer.detect_cycles()

        print("\nStage 3 completed successfully!")

    except ConfigError as e:
        print(f"Configuration error: {e}", file=sys.stderr)
        sys.exit(1)
    except DependencyFetchError as e:
        print(f"Dependency fetch error: {e}", file=sys.stderr)
        sys.exit(2)
    except CircularDependencyError as e:
        print(f"Circular dependency error: {e}", file=sys.stderr)
        sys.exit(3)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(4)


if __name__ == '__main__':
    main()
