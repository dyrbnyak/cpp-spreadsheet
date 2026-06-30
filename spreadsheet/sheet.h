#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

private:
    // Хранилище ячеек
    std::unordered_map<int, std::unique_ptr<Cell>> cells_;
    
    // Для быстрого получения размера печатной области
    mutable Size min_printing_area_{0, 0};
    mutable bool min_printing_area_valid_ = false;
    
    // Максимальные значения для печатной области
    Size max_printing_area_{-1, -1};



    // Вспомогательные методы
    int GetCellKey(Position pos) const;
    void InvalidateMinPrintingArea();
    void UpdateMinPrintingArea() const;
    void RecalculateMaxValues();

    // Создает пустую ячейку, если её нет
    void EnsureCellExists(Position pos);
    
    // Методы для работы с графом зависимостей
    //Здесь собираем всю интересную/полезную инфу, старт позиции, все зависиомсти, граф строим и передаем в CheckCycle
    bool HasCycle(const Position& start, const std::vector<Position>& dependencies) const;

    
    bool CheckCycle(const Position& start,
                    std::unordered_map<Position, int>& state,
                    std::unordered_map<Position, std::unordered_set<Position>>& graph) const;

    void InvalidateCache(const Position& pos);

    void UpdateDependencies(const Position& pos, 
                           const std::vector<Position>& old_deps,
                           const std::vector<Position>& new_deps);
};
