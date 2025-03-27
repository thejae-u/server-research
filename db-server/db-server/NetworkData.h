#ifdef _WIN32
#define MYLIBRARY_API __declspec(dllexport)
#else
#define MYLIBRARY_API
#endif

#include <iostream>

extern "C" {

    // 열거형 정의
    enum class ENetworkType{
        OPTION_ONE,
        OPTION_TWO,
        OPTION_THREE
    };

    // 구조체 정의
    struct MYLIBRARY_API SNetworkData{
        ENetworkType type;
        std::size_t bufSize;
        std::string data;
    };

    //// 클래스 정의
    //class MYLIBRARY_API MyClass {
    //public:
    //    MyClass();
    //    ~MyClass();
    //    void PrintInfo(const MyStruct& data);
    //};

    //// 객체 생성 및 삭제 함수
    //MYLIBRARY_API MyClass* CreateInstance();
    //MYLIBRARY_API void DestroyInstance(MyClass* instance);

} // extern "C"

