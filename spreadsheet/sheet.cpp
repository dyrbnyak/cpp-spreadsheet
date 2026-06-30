#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>

using namespace std::literals;

Sheet::~Sheet() = default;

int Sheet::GetCellKey(Position pos) const {    
    // Преобразуем позицию в уникальный ключ для хеш-таблицы
    return pos.row * Position::MAX_COLS + pos.col;
}

void Sheet::InvalidateMinPrintingArea() {
    min_printing_area_valid_ = true;
}

void Sheet::UpdateMinPrintingArea() const {
        // Обновляем только в тех случаях, когда инвалидирована печатная зона
    if (!min_printing_area_valid_) {
        return;
    }

    if (cells_.empty() || max_printing_area_.rows == -1 || max_printing_area_.cols == -1) {
        min_printing_area_ = Size{0, 0};

    } else {
        min_printing_area_ = Size{max_printing_area_.rows + 1, max_printing_area_.cols + 1};
    }
    //Инвалидация минимальной печатной области. Через метод не удается. mutable игнорируется
        min_printing_area_valid_ = false;
}

void Sheet::RecalculateMaxValues() {    
    // Сбрасываем значения
    max_printing_area_ = {.rows = -1, .cols = -1};

    for (const auto& [key, cell] : cells_) {
        if (cell && !cell->IsEmpty()) {
            // Перебираем все ячейки и конвертируем значения, чтоб получить новые максимальные значения
            int row = key / Position::MAX_COLS;
            int col = key % Position::MAX_COLS;

            max_printing_area_.rows = std::max(max_printing_area_.rows, row);
            max_printing_area_.cols = std::max(max_printing_area_.cols, col);
        }
    }

    InvalidateMinPrintingArea();
}

void Sheet::EnsureCellExists(Position pos){
    if (!pos.IsValid()) {
        return;
    }

    int key = GetCellKey(pos);
    auto it = cells_.find(key);

    if (it == cells_.end()) {
        // Создаем пустую ячейку
        auto cell = std::make_unique<Cell>(*this);
        cells_[key] = std::move(cell);
    }
}

bool Sheet::CheckCycle(const Position& start,
                       std::unordered_map<Position, int>& state,
                       std::unordered_map<Position, std::unordered_set<Position>>& graph) const {
    // Состояния: 0 - не посещена, 1 - в процессе, 2 - обработана

    std::vector<std::pair<Position, size_t>> stack; // вершина и индекс следующего соседа
    stack.push_back({start, 0});
    state[start] = 1;

    while (!stack.empty()) {
        auto& [current, next_idx] = stack.back();

        auto it = graph.find(current);

        if (it != graph.end()) {
            const auto& neighbors = it->second;

            if (next_idx < neighbors.size()) {
                // Получаем следующего соседа
                auto neighbor_it = neighbors.begin();
                std::advance(neighbor_it, next_idx);

                Position next = *neighbor_it;
                next_idx++;

                if (state[next] == 1) {
                    return true; // Найден цикл
                }

                if (state[next] == 0) {
                    state[next] = 1;
                    stack.push_back({next, 0});
                }
            } else {
                // Все соседи обработаны
                state[current] = 2;
                stack.pop_back();
            }
        } else {
            // Нет соседей
            state[current] = 2;
            stack.pop_back();
        }
    }

    return false;
}

bool Sheet::HasCycle(const Position& start, const std::vector<Position>& dependencies) const {
    // Строим полный граф зависимостей
    std::unordered_map<Position, std::unordered_set<Position>> graph;
    std::unordered_map<Position, int> state;

    // Добавляем все существующие ячейки и их зависимости
    for (const auto& [key, cell] : cells_) {
        int row = key / Position::MAX_COLS;
        int col = key % Position::MAX_COLS;
        Position pos{row, col};

        if (cell && !cell->IsEmpty()) {
            graph[pos] = cell->GetOutgoingEdges();
        } else {
            graph[pos] = {};
        }
        state[pos] = 0;
    }

    // Добавляем все зависимости, даже если их нет в cells_
    // (они были созданы через EnsureCellExists)
    for (const auto& dep : dependencies) {
        if (dep.IsValid() && graph.count(dep) == 0) {
            graph[dep] = {};
            state[dep] = 0;
        }
    }

    // Добавляем start, если его нет
    if (graph.count(start) == 0) {
        graph[start] = {};
        state[start] = 0;
    }

    // Добавляем новые зависимости для start
    for (const auto& dep : dependencies) {
        if (dep.IsValid()) {
            graph[start].insert(dep);
        }
    }

    // Проверяем циклы начиная с start
    return CheckCycle(start, state, graph);
}

void Sheet::InvalidateCache(const Position& pos) {
    std::vector<Position> queue;
    std::unordered_set<Position> visited;
    
    queue.push_back(pos);
    visited.insert(pos);
    
    size_t index = 0;
    while (index < queue.size()) {
        Position current = queue[index++];
        
        // Инвалидируем кэш текущей ячейки
        int key = GetCellKey(current);
        auto it = cells_.find(key);

        if (it != cells_.end() && it->second) {
            it->second->InvalidateCache();
        }
        
        // Добавляем все ячейки, которые зависят от текущей
        if (it != cells_.end() && it->second) {
            for (const auto& incoming : it->second->GetIncomingEdges()) {
                if (visited.find(incoming) == visited.end()) {
                    visited.insert(incoming);
                    queue.push_back(incoming);
                }
            }
        }
    }
}

void Sheet::UpdateDependencies(const Position& pos,
                               const std::vector<Position>& old_deps,
                               const std::vector<Position>& new_deps) {
    // Удаляем старые зависимости
    for (const auto& dep : old_deps) {
        if (dep.IsValid()) {
            int dep_key = GetCellKey(dep);
            auto dep_it = cells_.find(dep_key);

            if (dep_it != cells_.end() && dep_it->second) {
                dep_it->second->RemoveIncomingEdge(pos);
            }
        }
    }

    // Добавляем новые зависимости
    for (const auto& dep : new_deps) {
        if (dep.IsValid()) {
            int dep_key = GetCellKey(dep);
            auto dep_it = cells_.find(dep_key);

            if (dep_it != cells_.end() && dep_it->second) {
                dep_it->second->AddIncomingEdge(pos);
            }
        }
    }

    // Обновляем исходящие ребра у текущей ячейки
    int key = GetCellKey(pos);
    auto it = cells_.find(key);

    if (it != cells_.end() && it->second) {
        // Очищаем старые исходящие ребра
        for (const auto& dep : old_deps) {
            if (dep.IsValid()) {
                it->second->RemoveOutgoingEdge(dep);
            }
        }
        // Добавляем новые исходящие ребра
        for (const auto& dep : new_deps) {
            if (dep.IsValid()) {
                it->second->AddOutgoingEdge(dep);
            }
        }
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    int key = GetCellKey(pos);
    auto it = cells_.find(key);

    // Сохраняем старые зависимости
    std::vector<Position> old_deps;
    if (it != cells_.end() && it->second && !it->second->IsEmpty()) {
        old_deps = it->second->GetReferencedCells();
    }

    // Создаем временную ячейку для проверки
    std::unique_ptr<Cell> temp_cell = std::make_unique<Cell>(*this);

    try {
        temp_cell->Set(text);
    } catch (const FormulaException& e) {
        throw;
    }

    // Получаем новые зависимости
    std::vector<Position> new_deps;
    if (!temp_cell->IsEmpty()) {
        new_deps = temp_cell->GetReferencedCells();
    }

    // Проверяем на циклы ТОЛЬКО для формульных ячеек
    if (temp_cell->IsFormula() && !new_deps.empty()) {
        // Строим граф с учётом новой ячейки и её зависимостей

        // Сначала создаём все зависимые ячейки (пустые), чтобы они были в cells_
        // Это нужно, потому что зависимости могут ссылаться на несуществующие ячейки
        for (const auto& dep : new_deps) {
            if (dep.IsValid()) {
                EnsureCellExists(dep);
            }
        }

        // Теперь строим граф из всех ячеек, включая новые
        std::unordered_map<Position, std::unordered_set<Position>> graph;
        std::unordered_map<Position, int> state;

        // Добавляем все существующие ячейки
        for (const auto& [cell_key, cell] : cells_) {
            int row = cell_key / Position::MAX_COLS;
            int col = cell_key % Position::MAX_COLS;
            Position cell_pos{row, col};

            if (cell && !cell->IsEmpty()) {
                graph[cell_pos] = cell->GetOutgoingEdges();
            } else {
                graph[cell_pos] = {};
            }
            state[cell_pos] = 0;
        }

        // Добавляем новую ячейку (если её ещё нет в графе)
        if (graph.count(pos) == 0) {
            graph[pos] = {};
            state[pos] = 0;
        }

        // Добавляем все зависимости (теперь они должны существовать в cells_)
        for (const auto& dep : new_deps) {
            if (dep.IsValid()) {
                // Убеждаемся, что зависимость есть в графе
                if (graph.count(dep) == 0) {
                    // Создаём вершину для зависимости
                    graph[dep] = {};
                    state[dep] = 0;
                }
                // Добавляем ребро от pos к dep
                graph[pos].insert(dep);
            }
        }

        // Проверяем цикл
        if (CheckCycle(pos, state, graph)) {
            throw CircularDependencyException("Circular dependency detected");
        }
    }

    // создаём пустые ячейки для всех зависимостей (если ещё не созданы)
    for (const auto& dep : new_deps) {
        if (dep.IsValid()) {
            EnsureCellExists(dep);
        }
    }

    // Обновляем зависимости (теперь все ячейки существуют)
    // Важно: UpdateDependencies должен обновлять рёбра у ВСЕХ ячеек
    UpdateDependencies(pos, old_deps, new_deps);

    // Сохраняем ячейку
    cells_[key] = std::move(temp_cell);

    // Обновляем максимумы
    RecalculateMaxValues();

    // Инвалидируем кэш
    InvalidateCache(pos);
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    int key = GetCellKey(pos);
    auto it = cells_.find(key);

    if (it == cells_.end()) {
        return nullptr;
    }

    // Возвращаем даже пустую ячейку, если она существует
    return it->second.get();
}

CellInterface* Sheet::GetCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    int key = GetCellKey(pos);
    auto it = cells_.find(key);

    if (it == cells_.end()) {
        return nullptr;
    }

    return it->second.get();
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    int key = GetCellKey(pos);
    auto it = cells_.find(key);

    if (it != cells_.end() && !it->second->IsEmpty()) {
        // Сохраняем зависимости для удаления
        auto deps = it->second->GetReferencedCells();
        
        // Удаляем входящие ребра у зависимостей
        for (const auto& dep : deps) {
            if (dep.IsValid()) {
                int dep_key = GetCellKey(dep);
                auto dep_it = cells_.find(dep_key);

                if (dep_it != cells_.end() && dep_it->second) {
                    dep_it->second->RemoveIncomingEdge(pos);
                }
            }
        }
        
        // Очищаем ячейку
        it->second->Clear();
        
        // Удаляем пустую ячейку из хранилища
        cells_.erase(it);
        
        // Пересчитываем максимумы
        RecalculateMaxValues();
        
        // Инвалидируем кэш
        InvalidateCache(pos);
    }
}

Size Sheet::GetPrintableSize() const {
    UpdateMinPrintingArea();
    return min_printing_area_;
}

void Sheet::PrintValues(std::ostream& output) const {
    UpdateMinPrintingArea();

    Size size = min_printing_area_;

    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }

            Position pos{row, col};
            const CellInterface* cell = GetCell(pos);

            if (cell) {
                auto value = cell->GetValue();

                std::visit([&output](const auto& v) {
                    output << v;
                }, value);
            }
            // Пустая ячейка - ничего не выводим
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    UpdateMinPrintingArea();

    Size size = min_printing_area_;

    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }

            Position pos{row, col};
            const CellInterface* cell = GetCell(pos);

            if (cell) {
                output << cell->GetText();
            }
            // Пустая ячейка - ничего не выводим
        }
        output << '\n';
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
