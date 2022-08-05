#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (object) {
        if ((object.TryAs<Bool>() != nullptr) &&
             object.TryAs<Bool>()->GetValue()) {
            return true;
        }
        if ((object.TryAs<Number>() != nullptr) &&
            (object.TryAs<Number>()->GetValue() != 0)) {
            return true;
        }
        if ((object.TryAs<String>() != nullptr) &&
            !object.TryAs<String>()->GetValue().empty()) {
            return true;
        }
    }

    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__"s, 0)) {
        // если есть специальный метод - вызываем его
        Call("__str__"s, {}, context)->Print(os, context);
    }
    else {
        // печать адреса
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const Method* p_method = class_.GetMethod(method);
    if (nullptr != p_method) {
        if (argument_count == p_method->formal_params.size()) {
            return true;
        }
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return fields_;
}

const Closure& ClassInstance::Fields() const {
    return fields_;
}

ClassInstance::ClassInstance(const Class& cls) : class_(cls) {
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (HasMethod(method, actual_args.size())) {
        Closure closure;
        closure["self"s] = ObjectHolder::Share(*this);
        const Method* p_method = class_.GetMethod(method);
        size_t param_size = p_method->formal_params.size();
        for (size_t i = 0; i < param_size; ++i) {
            closure[p_method->formal_params[i]] = actual_args[i];
        }
        return p_method->body.get()->Execute(closure, context);
    }
    else {
        throw std::runtime_error("Call for an undefined method \"" + method + "\"");
    }
}

Class::Class(std::string name,
             std::vector<Method> methods,
             const Class* parent) : name_(name),
                                    methods_(move(methods)),
                                    parent_(parent) {
    if (nullptr != parent_) {
        for (const auto& method : parent_->methods_) {
            // заполняем таблицу функций методами родителя
            v_table_[method.name] = &method;
        }
    }
    for (const auto& method : methods_) {
        // заполняем таблицу функций своими методами
        v_table_[method.name] = &method;
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    if (v_table_.count(name) != 0) {
        // т.к. используем мапу то берем метод за О(1)
        return v_table_.at(name);
    }
    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "sv << GetName();
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!lhs && !rhs) {
        // оба None
        return true;
    }

    auto c_obj_lhs = lhs.TryAs<ClassInstance>();
    if (nullptr != c_obj_lhs) {
        if (c_obj_lhs->HasMethod("__eq__"s, 1)) {
            ObjectHolder obj;
            obj = c_obj_lhs->Call("__eq__"s, {rhs}, context);
            return obj.TryAs<Bool>()->GetValue();
        }
    }

    auto b_obj_lhs = lhs.TryAs<Bool>();
    auto b_obj_rhs = rhs.TryAs<Bool>();
    if ((nullptr != b_obj_lhs) && (nullptr != b_obj_rhs)) {
        return b_obj_lhs->GetValue() == b_obj_rhs->GetValue();
    }

    auto n_obj_lhs = lhs.TryAs<Number>();
    auto n_obj_rhs = rhs.TryAs<Number>();
    if ((nullptr != n_obj_lhs) && (nullptr != n_obj_rhs)) {
        return n_obj_lhs->GetValue() == n_obj_rhs->GetValue();
    }

    auto s_obj_lhs = lhs.TryAs<String>();
    auto s_obj_rhs = rhs.TryAs<String>();
    if ((nullptr != s_obj_lhs) && (nullptr != s_obj_rhs)) {
        return s_obj_lhs->GetValue() == s_obj_rhs->GetValue();
    }

    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    auto c_obj_lhs = lhs.TryAs<ClassInstance>();
    if (nullptr != c_obj_lhs) {
        if (c_obj_lhs->HasMethod("__lt__"s, 1)) {
            ObjectHolder obj;
            obj = c_obj_lhs->Call("__lt__"s, {rhs}, context);
            return obj.TryAs<Bool>()->GetValue();
        }
    }

    auto b_obj_lhs = lhs.TryAs<Bool>();
    auto b_obj_rhs = rhs.TryAs<Bool>();
    if ((nullptr != b_obj_lhs) && (nullptr != b_obj_rhs)) {
        return b_obj_lhs->GetValue() < b_obj_rhs->GetValue();
    }

    auto n_obj_lhs = lhs.TryAs<Number>();
    auto n_obj_rhs = rhs.TryAs<Number>();
    if ((nullptr != n_obj_lhs) && (nullptr != n_obj_rhs)) {
        return n_obj_lhs->GetValue() < n_obj_rhs->GetValue();
    }

    auto s_obj_lhs = lhs.TryAs<String>();
    auto s_obj_rhs = rhs.TryAs<String>();
    if ((nullptr != s_obj_lhs) && (nullptr != s_obj_rhs)) {
        return s_obj_lhs->GetValue() < s_obj_rhs->GetValue();
    }

    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !(Less(lhs, rhs, context) || Equal(lhs, rhs, context));
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return (Less(lhs, rhs, context) || Equal(lhs, rhs, context));
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime