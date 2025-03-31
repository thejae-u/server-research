#ifdef _WIN32
#define MYLIBRARY_API __declspec(dllexport)
#else
#define MYLIBRARY_API
#endif

#include <iostream>

extern "C" {

    // 열거형 정의
    enum class ENetworkType{
        // User Data Type
        LOGIN = 0,
        REGISTER,
        RETRIEVE,
        ACCESS,
        LOGOUT,

		// System Data Type
        SYSTMEM_OPTION_1 = 100,
		SYSTEM_OPTION_2,
		SYSTEM_OPTION_3,

        ADMIN_SERVER_OFF = 900,
        ADMIN_SERVER_ON,
		ADMIN_SERVER_REBOOT,
    };

    enum class ELoginError
    {
        USER_NOT_FOUND,
        USER_ALREADY_EXIST,
        INCORRECT,
    };

    // 구조체 정의
    struct MYLIBRARY_API SNetworkData{
        std::string uuid;
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

