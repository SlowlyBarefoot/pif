# PIF

PIF(Platform-independent framework)는 각종 디바이스를 제어하는 프레임워크로써 플랫폼에 비종속적인 코드만으로 구성된다. 그래서 모든 플랫폼에서 사용 가능하게 구현하는 것을 목적으로 한다. 

PIF의 기본 방침.
1. 메모리 할당은 메인 루프 진입전 초기 동작에서만 실행한다.
2. C로 작성되었기에 구조체내 선언된 변수의 접근 권한을 부여할 수 없다. 그래서 private 변수는 변수명 앞에 __를 붙이고 외부에서 이 변수를 되도록 변경하지 않도록 한다.
3. 모든 디바이스에 디바이스 코드를 부여하여 구분 가능하게 한다.

PIF는 플랫폼에 종속적인 코드와 application 사이에 존재한다. 그래서 이 두 개의 층과 연결 방법이 필요하다.
1. 플랫폼에 종속적인 코드 -> PIF : signal. PIF 함수명앞에 sig를 붙인 함수.
2. PIF -> 플랫폼에 종속적인 코드 : action. 함수 포인터로써 변수명 앞에 act가 붙어 있다. 
3. Application -> PIF : PIF의 일반 함수.
4. PIF -> Application : event. 함수 포인터로써 변수명 앞에 evt가 붙어 있다.


---


PIF(Platform-independent framework) is a framework for controlling various devices and consists of only non-continuous code on the platform. So the goal is to implement it as available on all platforms. 

PIF's basic policy.
1. Memory allocation is performed only in the initial operation before entering the main roof.
2. Because it is written in C, access to declared variables within the structure cannot be granted. Thus, the private variable is prefixed with __ before the variable name and does not change it externally.
3. Device code is assigned to all devices for differentiation.

PIF exists between the application and the code dependent on the platform. So we need these two layers and a connection method.
1. Code dependent on the platform -> PIF : signal. PIF Function with sig before function name.
2. Code dependent on the PIF -> platform: action. Function pointer with an action before the variable name. 
3. Application -> PIF : General function of PIF.
4. PIF -> Application: event. As a function pointer, evt is attached in front of the variable name.
