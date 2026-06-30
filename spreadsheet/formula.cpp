#include "formula.h"

#include "FormulaAST.h"
#include "cell.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, const FormulaError& fe) {
    return output << fe.ToString();
}

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression) try
        : ast_(ParseFormulaAST(expression)) {
        // Тело конструктора выполняется только если парсинг прошёл успешно

    } catch (const FormulaException& e) {
        // Перебрасываем исключение дальше, так как это синтаксическая ошибка
        throw;

    } catch (const std::exception& e) {
        // Любые другие ошибки парсинга превращаем в FormulaException
        throw FormulaException(e.what());
    }

    Cell::Value Evaluate(const SheetInterface& sheet) const override {
        try {
            double result = ast_.Evaluate(sheet);
            return result;

        } catch (const FormulaError& e) {
            return e;

        } catch (const std::exception& e) {
            return FormulaError(FormulaError::Category::Arithmetic);
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        std::vector<Position> result{};

        const auto& cells = ast_.GetCells();

        if(!cells.empty()){
            result.reserve(std::distance(cells.begin(), cells.end()));

            for (const auto& pos : cells) {
                result.push_back(pos);
            }

            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());
        }

        return result;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}
