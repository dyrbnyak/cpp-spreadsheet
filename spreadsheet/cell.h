#pragma once

#include "common.h"
#include "formula.h"

#include <memory>
#include <string>
#include <variant>
#include <unordered_set>
#include <optional>

class Cell : public CellInterface {
public:
    explicit Cell(const SheetInterface& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    void InvalidateCache();

    // Методы для работы с графом зависимостей
    void AddOutgoingEdge(const Position& pos);
    void RemoveOutgoingEdge(const Position& pos);

    void AddIncomingEdge(const Position& pos);
    void RemoveIncomingEdge(const Position& pos);
    
    const std::unordered_set<Position>& GetOutgoingEdges() const { return outgoing_edges_; }
    const std::unordered_set<Position>& GetIncomingEdges() const { return incoming_edges_; }

    void SetOutgoingEdges(const std::unordered_set<Position>& edges) {
        outgoing_edges_ = edges;
    }
    
    bool IsEmpty() const { return impl_->IsEmpty(); }
    bool IsFormula() const { return impl_->IsFormula(); }

private:
    class Impl {
    public:
        virtual ~Impl() = default;
        virtual Value GetValue(const SheetInterface& sheet) const = 0;
        virtual std::string GetText() const = 0;
        virtual std::vector<Position> GetReferencedCells() const = 0;
        virtual bool IsEmpty() const = 0;
        virtual bool IsFormula() const = 0;
    };

    class EmptyImpl : public Impl {
    public:
        Value GetValue(const SheetInterface& /* sheet */) const override;
        std::string GetText() const override;
        std::vector<Position> GetReferencedCells() const override;
        bool IsEmpty() const override { return true; }
        bool IsFormula() const override { return false; }
    };

    class TextImpl : public Impl {
    public:
        explicit TextImpl(std::string text);
        Value GetValue(const SheetInterface& /* sheet */) const override;
        std::string GetText() const override;
        std::vector<Position> GetReferencedCells() const override;
        bool IsEmpty() const override { return false; }
        bool IsFormula() const override { return false; }

    private:
        std::string text_;
        std::string value_text_;
    };

    class FormulaImpl : public Impl {
    public:
        explicit FormulaImpl(std::string expression);
        Value GetValue(const SheetInterface& sheet) const override;
        std::string GetText() const override;
        std::vector<Position> GetReferencedCells() const override;
        bool IsEmpty() const override { return false; }
        bool IsFormula() const override { return true; }

    private:
        std::unique_ptr<FormulaInterface> formula_;
        std::string expression_;
    };

    static std::unique_ptr<Impl> CreateImpl(const std::string& text);

    const SheetInterface& sheet_;

    std::unique_ptr<Impl> impl_;
    mutable std::optional<Value> cache_;
    mutable bool cache_valid_ = true;

    // Исходящие рёбра - ячейки, от которых зависит данная
    std::unordered_set<Position> outgoing_edges_;
    // Входящие рёбра - ячейки, которые зависят от данной
    std::unordered_set<Position> incoming_edges_;
};
