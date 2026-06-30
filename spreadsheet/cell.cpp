#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

Cell::Cell(const SheetInterface& sheet) 
    : sheet_(sheet)
    , impl_(std::make_unique<EmptyImpl>()) {
}

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    auto new_impl = CreateImpl(text);

    impl_ = std::move(new_impl);
    cache_valid_ = false;
    cache_.reset();

    // Обновляем исходящие рёбра
    outgoing_edges_.clear();
    if (impl_->IsFormula()) {

        auto deps = impl_->GetReferencedCells();

        for (const auto& dep : deps) {
            outgoing_edges_.insert(dep);
        }
    }
}

void Cell::Clear() {
    impl_ = std::make_unique<EmptyImpl>();

    cache_valid_ = false;
    cache_.reset();
    outgoing_edges_.clear();
}
Cell::Value Cell::GetValue() const {
    if (cache_valid_ && cache_.has_value()) {
        return *cache_;
    }
    
    try {
        Value result = impl_->GetValue(sheet_);

        cache_ = result;
        cache_valid_ = true;

        return result;

    } catch (const FormulaError& e) {
        cache_ = e;
        cache_valid_ = true;

        return e;

    } catch (const std::exception&) {
        FormulaError error(FormulaError::Category::Arithmetic);
        cache_ = error;
        cache_valid_ = true;

        return error;
    }
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

void Cell::InvalidateCache() {
    cache_valid_ = false;
    cache_.reset();
}

std::unique_ptr<Cell::Impl> Cell::CreateImpl(const std::string& text) {
    if (text.empty()) {
        return std::make_unique<EmptyImpl>();
    }

    if (text.size() > 1 && text[0] == FORMULA_SIGN) {
        try {
            std::string expression = text.substr(1);
            return std::make_unique<FormulaImpl>(std::move(expression));
        } catch (const FormulaException&) {
            throw;
        } catch (const std::exception&) {
            throw FormulaException("Failed to create formula");
        }
    }

    return std::make_unique<TextImpl>(text);
}

void Cell::AddOutgoingEdge(const Position& pos) {
    outgoing_edges_.insert(pos);
}

void Cell::RemoveOutgoingEdge(const Position& pos) {
    outgoing_edges_.erase(pos);
}

void Cell::AddIncomingEdge(const Position& pos) {
    incoming_edges_.insert(pos);
}

void Cell::RemoveIncomingEdge(const Position& pos) {
    incoming_edges_.erase(pos);
}

// Реализация EmptyImpl
Cell::Value Cell::EmptyImpl::GetValue(const SheetInterface& /* sheet */) const {
    return std::string("");
}

std::string Cell::EmptyImpl::GetText() const {
    return "";
}

std::vector<Position> Cell::EmptyImpl::GetReferencedCells() const {
    return {};
}

// Реализация TextImpl
Cell::TextImpl::TextImpl(std::string text)
    : text_(std::move(text)) {
    if (!text_.empty() && text_[0] == ESCAPE_SIGN) {
        value_text_ = text_.substr(1);

    } else {
        value_text_ = text_;
    }
}

Cell::Value Cell::TextImpl::GetValue(const SheetInterface& /* sheet */) const {
    return value_text_;
}

std::string Cell::TextImpl::GetText() const {
    return text_;
}

std::vector<Position> Cell::TextImpl::GetReferencedCells() const {
    return {};
}

// Реализация FormulaImpl
Cell::FormulaImpl::FormulaImpl(std::string expression)
    : expression_(std::move(expression)) {
    formula_ = ParseFormula(expression_);
}

Cell::Value Cell::FormulaImpl::GetValue(const SheetInterface& sheet) const {
    if (!formula_) {
        return FormulaError(FormulaError::Category::Arithmetic);
    }

    try {
        Cell::Value result = formula_->Evaluate(sheet);
        return result;

    } catch (const FormulaError& e) {
        return e;

    } catch (const std::exception&) {
        return FormulaError(FormulaError::Category::Arithmetic);
    }
}

std::string Cell::FormulaImpl::GetText() const {
    return std::string(1, FORMULA_SIGN) + formula_->GetExpression();
}

std::vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
    if (!formula_) {
        return {};
    }
    
    return formula_->GetReferencedCells();
}
