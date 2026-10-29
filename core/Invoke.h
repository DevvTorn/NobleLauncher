#pragma once

#include <QMetaObject>
#include <QThread>

#include <future>
#include <utility>

namespace Noble {

/** Выполняет лямбду в потоке объекта (с ожиданием результата), безопасно из рабочих потоков. */
template <typename T, typename F>
auto invokeBlocking(T *object, F &&fn) -> decltype(fn())
{
    if (!object || QThread::currentThread() == object->thread())
        return fn();
    std::promise<decltype(fn())> promise;
    auto future = promise.get_future();
    QMetaObject::invokeMethod(object, [&promise, fn] {
        try {
            promise.set_value(fn());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }, Qt::BlockingQueuedConnection);
    return future.get();
}

} // namespace Noble
