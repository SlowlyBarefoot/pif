# PIF

PIF(Platform-independent framework)는 각종 디바이스를 제어하는 프레임워크로써 플랫폼에 비종속적인 코드만으로 구성된다. 그래서 모든 플랫폼에서 사용 가능하게 구현하는 것을 목적으로 한다. 

PIF의 기본 방침.
1. 메모리 할당은 메인 루프 진입전 초기 동작에서만 실행한다.
2. C로 작성되었기에 구조체내 선언된 변수의 접근 권한을 부여할 수 없다. 
   그래서 private 변수는 변수명 앞에 __를 붙이고 외부에서 이 변수를 되도록 읽거나 쓰지 않도록 한다.
   또한 읽기 전용 변수명 앞에는 _를 붙이고 외부에서 이 변수를 되도록 변경하지 않도록 한다.
3. 모든 디바이스에 디바이스 코드를 부여하여 관리할 수 있게 한다.
4. Main Loop에서 동작하는 기능들을 Task로 분류하고 각 Task는 동작 주기를 설정할 수 있게하여 MCU를 효율을 높인다.

PIF는 플랫폼에 종속적인 코드와 application 사이에 존재한다. 그래서 이 두 개의 층과 연결하는 방법이 필요하다.
1. 플랫폼에 종속적인 코드 -> PIF : signal. PIF 함수명앞에 sig를 붙인 함수.
2. PIF -> 플랫폼에 종속적인 코드 : action. 함수 포인터로써 변수명 앞에 act가 붙어 있다. 
                            : event. 함수 포인터로써 변수명 앞에 evt가 붙어 있다.
3. Application -> PIF : PIF의 일반 함수.
4. PIF -> Application : event. 함수 포인터로써 변수명 앞에 evt가 붙어 있다.

이 framework를 사용한 예제는 아래 주소를 참조한다.

https://github.com/SlowlyBarefoot/pif-example

---

PIF (Platform-independent framework) is a framework that controls various devices, and consists of only code independent of the platform. So, it aims to be implemented so that it can be used on all platforms.

PIF's basic policy.
1. Memory allocation is executed only in the initial operation before entering the main loop.
2. Since it is written in C, it is not possible to grant access rights to variables declared in the structure.
   So, for private variables, prefix the variable name with __ and avoid reading or writing this variable from outside.
   Also, add _ in front of the name of a read-only variable, and do not change this variable externally.
3. Assign device codes to all devices so they can be managed.
4. The functions that operate in the main loop are classified into tasks, and each task can set the operation period to increase the efficiency of the MCU.

PIF exists between platform-dependent code and application. So we need a way to connect these two layers.
1. Platform dependent code -> PIF: signal. A function with sig in front of the PIF function name.
2. PIF -> Platform dependent code: action. As a function pointer, act is attached in front of the variable name.
                            : event. As a function pointer, evt is attached in front of the variable name.
3. Application -> PIF: General function of PIF.
4. PIF -> Application: event. As a function pointer, evt is attached in front of the variable name.

See the address below for an example using this framework.

https://github.com/SlowlyBarefoot/pif-example
