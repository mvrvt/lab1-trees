#pragma once

#include <stdexcept> // std::invalid_argument
#include <string>

// ========== Порядки обхода бинарного дерева ==========
// Обход - это посещение каждого узла ровно один раз. В каждом узле нужно сделать
// три действия: посетить сам узел (К - корень), обойти левое поддерево (Л)
// и обойти правое поддерево (П). Порядок этих трёх действий и задаёт обход.
// Всего перестановок 3! = 6:
//
//   КЛП (NLR) - прямой (pre-order)      ЛКП (LNR) - симметричный (in-order):
//   КПЛ (NRL)                                       для дерева поиска даёт значения по возрастанию
//   ЛПК (LRN) - обратный (post-order)   ПКЛ (RNL) - значения по убыванию
//   ПЛК (RLN)

// Одно действие в узле
enum class TraversalStep {
    kNode,  // К: посетить узел
    kLeft,  // Л: обойти левое поддерево
    kRight, // П: обойти правое поддерево
};

enum class TraversalOrder {
    kNLR, // КЛП
    kNRL, // КПЛ
    kLNR, // ЛКП
    kLRN, // ЛПК
    kRNL, // ПКЛ
    kRLN, // ПЛК
};

// Три действия обхода в нужном порядке
struct TraversalSteps {
    TraversalStep steps[3];
};

inline TraversalSteps GetTraversalSteps(TraversalOrder order) {
    using S = TraversalStep;
    switch (order) {
    case TraversalOrder::kNLR: return {{S::kNode, S::kLeft, S::kRight}};
    case TraversalOrder::kNRL: return {{S::kNode, S::kRight, S::kLeft}};
    case TraversalOrder::kLNR: return {{S::kLeft, S::kNode, S::kRight}};
    case TraversalOrder::kLRN: return {{S::kLeft, S::kRight, S::kNode}};
    case TraversalOrder::kRNL: return {{S::kRight, S::kNode, S::kLeft}};
    case TraversalOrder::kRLN: return {{S::kRight, S::kLeft, S::kNode}};
    }
    throw std::invalid_argument("GetTraversalSteps: unknown order"); // сюда не попадём
}

// "КЛП", "КПЛ", ... 
inline std::string ToString(TraversalOrder order) {
    switch (order) {
    case TraversalOrder::kNLR: return "КЛП";
    case TraversalOrder::kNRL: return "КПЛ";
    case TraversalOrder::kLNR: return "ЛКП";
    case TraversalOrder::kLRN: return "ЛПК";
    case TraversalOrder::kRNL: return "ПКЛ";
    case TraversalOrder::kRLN: return "ПЛК";
    }
    throw std::invalid_argument("ToString: unknown order");
}

// Обратное к ToString. Понимает и русские ("ЛКП"), и латинские ("LNR") обозначения.
// Для любой другой строки бросает std::invalid_argument.
inline TraversalOrder ParseTraversalOrder(const std::string& text) {
    const TraversalOrder all_orders[] = {
        TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
        TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
    };
    const char* latin_names[] = {"NLR", "NRL", "LNR", "LRN", "RNL", "RLN"};
    for (int i = 0; i < 6; ++i) {
        if (text == ToString(all_orders[i]) || text == latin_names[i])
            return all_orders[i];
    }
    throw std::invalid_argument("ParseTraversalOrder: unknown order '" + text + "'");
}
