import json
import argparse
import sys
import urllib.request
import urllib.error
from collections import deque
from typing import Dict, Any, List
from urllib.parse import urlparse

# Библиотеки для визуализации
try:
    import matplotlib.pyplot as plt
    import networkx as nx
except ImportError:
    print("Error: matplotlib and networkx are required. Install with: pip install matplotlib networkx")
    sys.exit(1)


class ConfigError(Exception):
    pass


class DependencyFetchError(Exception):
    pass


class DependencyVisualizer:
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)
        self._validate_config()
        self.dependency_graph = {}

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
                return json.load(f)
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
            test_repo = self.load_test_repository()
            root_package = self.config['package_name']

            if root_package not in test_repo:
                raise DependencyFetchError(f"Package {root_package} not found in test repository")

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

    def generate_graphviz_dot(self) -> str:
        """Генерирует представление графа на языке Graphviz DOT"""
        dot_lines = ["digraph G {", "    rankdir=TB;", "    size=\"12,8\";"]

        # Добавляем корневой узел с другим цветом
        root_package = self.config['package_name']
        dot_lines.append(f'    "{root_package}" [shape=box, style=filled, fillcolor=lightgreen];')

        # Добавляем все узлы
        for package in self.dependency_graph:
            if package != root_package:
                dot_lines.append(f'    "{package}" [shape=box, style=filled, fillcolor=lightblue];')

        # Добавляем все ребра
        for package, dependencies in self.dependency_graph.items():
            for dep in dependencies:
                if dep in self.dependency_graph:
                    dot_lines.append(f'    "{package}" -> "{dep}";')

        dot_lines.append("}")
        return "\n".join(dot_lines)

    def save_graph_image(self, dot_source: str):
        """Сохраняет граф в виде PNG изображения используя networkx и matplotlib"""
        try:
            # Создаем граф networkx
            G = nx.DiGraph()

            # Добавляем узлы и ребра
            for package, dependencies in self.dependency_graph.items():
                G.add_node(package)
                for dep in dependencies:
                    if dep in self.dependency_graph:
                        G.add_edge(package, dep)

            # Определяем размер фигуры в зависимости от количества узлов
            num_nodes = len(G.nodes())
            fig_width = max(12, min(20, num_nodes))  # Ограничиваем максимальный размер
            fig_height = fig_width * 0.8

            # Создаем изображение с явно заданными осями
            fig, ax = plt.subplots(figsize=(fig_width, fig_height))

            # Используем spring layout для лучшего расположения узлов
            pos = nx.spring_layout(G, k=1.5, iterations=50, seed=42)  # seed для воспроизводимости

            # Определяем цвета узлов
            node_colors = []
            root_package = self.config['package_name']
            for node in G.nodes():
                if node == root_package:
                    node_colors.append('lightgreen')
                else:
                    node_colors.append('lightblue')

            # Рисуем граф
            nx.draw_networkx_nodes(G, pos,
                                   node_color=node_colors,
                                   node_size=1500,
                                   alpha=0.9,
                                   ax=ax)

            nx.draw_networkx_edges(G, pos,
                                   arrows=True,
                                   arrowsize=15,
                                   edge_color='gray',
                                   ax=ax)

            nx.draw_networkx_labels(G, pos,
                                    font_size=8,
                                    font_weight='bold',
                                    ax=ax)

            # Устанавливаем заголовок и убираем оси
            ax.set_title(f"Dependency Graph for {self.config['package_name']}", size=14, pad=20)
            ax.axis('off')

            # Вместо tight_layout используем adjust
            plt.subplots_adjust(left=0.1, right=0.9, top=0.9, bottom=0.1)

            # Сохраняем изображение
            plt.savefig(
                self.config['output_image'],
                format='PNG',
                dpi=100,
                bbox_inches=None,  # Не используем bbox_inches
                pad_inches=0.1
            )
            plt.close()

            print(f"Graph image saved as: {self.config['output_image']}")

        except Exception as e:
            print(f"Error saving graph image: {e}")

    def print_ascii_tree(self):
        """Выводит зависимости в виде ASCII-дерева"""
        if not self.dependency_graph:
            print("No dependencies to display")
            return

        def build_tree(node, visited=None, prefix="", is_last=True):
            if visited is None:
                visited = set()

            if node in visited:
                return

            visited.add(node)

            # Текущий узел
            connector = "└── " if is_last else "├── "
            print(prefix + connector + node)

            # Дочерние узлы
            children = self.dependency_graph.get(node, [])
            new_prefix = prefix + ("    " if is_last else "│   ")

            for i, child in enumerate(children):
                is_last_child = (i == len(children) - 1)
                if child not in visited:
                    build_tree(child, visited, new_prefix, is_last_child)
                else:
                    # Отмечаем циклические зависимости
                    cyclic_connector = "└── " if is_last_child else "├── "
                    print(new_prefix + cyclic_connector + child + " (cyclic)")

        print(f"\nASCII Tree for '{self.config['package_name']}':")
        build_tree(self.config['package_name'])


def main():
    parser = argparse.ArgumentParser(description='Dependency Graph Visualizer - Stage 5: Visualization (Fixed)')
    parser.add_argument('--config', default='config5.json', help='Path to config file (default: config5.json)')
    args = parser.parse_args()

    try:
        visualizer = DependencyVisualizer(args.config)
        visualizer.print_config()

        # Построение графа зависимостей
        visualizer.build_dependency_graph_bfs()

        # Генерация Graphviz DOT (для демонстрации)
        dot_source = visualizer.generate_graphviz_dot()
        print("\nGenerated Graphviz DOT representation:")
        print(dot_source)

        # Сохранение PNG изображения с помощью matplotlib
        visualizer.save_graph_image(dot_source)

        # Вывод ASCII-дерева если включено
        if visualizer.config['ascii_tree']:
            visualizer.print_ascii_tree()

        print("\nStage 5 completed successfully!")

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
