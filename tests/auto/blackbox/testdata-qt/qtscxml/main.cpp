#ifdef HAS_QTSCXML
#include <dummystatemachine.h>
#endif

#include <iostream>

int main()
{
#ifdef HAS_QTSCXML
    QbsTest::QbsStateMachine machine;
    std::cout << "state machine name: " << qPrintable(machine.name()) << std::endl;
#else
    std::cout << "QtScxml not present" << std::endl;
#endif
}
