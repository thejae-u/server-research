#pragma once
#include <memory>

template <class T>
class Base : public std::enable_shared_from_this<T>
{
public:
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual ~Base() = default;
};
