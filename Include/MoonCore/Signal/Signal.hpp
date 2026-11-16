#pragma once
#include "Core.hpp"
#include "Containers/String.hpp"
#include "Callables/Callable.hpp"
#include "Utility/TypeUtility.hpp"
#include "Logging/Logger.hpp"

#define MC_SIGNAL_CATEGORY(categories) \
    public: virtual ::MC::Signal::Category GetCategory() const noexcept override { return categories; }
#define MC_SIGNAL_TYPE(type)                                                             \
    public:                                                                               \
        static ::MC::Signal::Type GetStaticType() noexcept                \
        {                                                                                 \
            return ::MC::Signal::GenerateType<type>();                    \
        }                                                                                 \
                                                                                          \
        virtual ::MC::Signal::Type GetType() const noexcept override      \
        {                                                                                 \
            return type::GetStaticType();                                                 \
        }                                                                                 \
        virtual MC::StringView GetName() const noexcept override { return TEXT(#type); }
#define MC_SIGNAL_INHERIT() public MC::Signal

#define MC_FMT_SIGNAL(signal, str, ...) MC_FMT_FORMATTER((signal)&, "Signal %s fired! Data: " ## str, MC_FMT_FORMATTER_METHOD(GetName), __VA_ARGS__)
#define MC_FMT_DEFAULT_SIGNAL(signal) MC_FMT_FORMATTER((signal)&, "Signal %s fired!", MC_FMT_FORMATTER_METHOD(GetName))

namespace MC
{
    template<typename Function, typename Object>
    static constexpr auto BindObjectToSignal(Function&& function, Object* object) noexcept
        -> Callables::Binder<Function, Object*, Callables::PlaceholderT<1>>
    {
        return Callables::Bind(Forward<Function>(function), object, Callables::Placeholder<1>);
    }

    class Signal
    {
    public:
        using CallbackType = MC::Callables::Callable<void(MC::Signal&)>;
        
    public:
        using Type = u32;
        using Category = u64;

    private:
        using TypeGenerator = Utility::TypeIDGenerator<Type, Type>;
        using CategoryGenerator = Utility::TypeIDGenerator<Category, Category>;

    public:
        template<typename T> static constexpr Type GenerateType() noexcept { return TypeGenerator::ID<T>(); }
        template<typename T> static constexpr Category GenerateCategory() noexcept { return CategoryGenerator::ID<T>(); }

    public:
        virtual ~Signal() = default;

        virtual Category GetCategory() const noexcept = 0;
        virtual Type GetType() const noexcept = 0;
        virtual MC::StringView GetName() const noexcept = 0;

    public:
        bool IsHandled() const noexcept { return m_IsHandled; }
        bool IsInCategory(Category category) const noexcept { return GetCategory() & category; }

    private:
        bool m_IsHandled = false;
        friend class SignalDispatcher;
    };
    
    class SignalDispatcher
    {
    public:
        SignalDispatcher(Signal& signal) : m_Signal(signal) {}
    
        template<typename T, typename F>
        bool Dispatch(F&& function) noexcept(IsNothrowInvocable<F, T&>)
        {
            if (m_Signal.GetType() == T::GetStaticType())
            {
                m_Signal.m_IsHandled = Invoke(Forward<F>(function), static_cast<T&>(m_Signal));
                return true;
            }
            return false;
        }
    
    private:
        Signal& m_Signal;
    };
}

MC_FMT_FORMATTER(MC::Signal&, "Signal %s fired!", MC_FMT_FORMATTER_METHOD(GetName))