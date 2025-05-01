#include "ConsoleProgressIndicator.hxx"
#include "MainWindow.h"

IMPLEMENT_STANDARD_RTTIEXT(ConsoleProgressIndicator, Message_ProgressIndicator)

ConsoleProgressIndicator::ConsoleProgressIndicator()
    :myLastShownProgress(-1)
{
    // Initialization if needed
}

void ConsoleProgressIndicator::Show(const Message_ProgressScope& theScope, const Standard_Boolean theForce)
{
    Standard_Real pos = GetPosition() * 100.0;
    if (static_cast<int>(pos) != myLastShownProgress || theForce)
    {
        //std::cout << "\rProgress: " << static_cast<int>(pos) << "%" << std::flush;
        myLastShownProgress = static_cast<int>(pos);
		MainWindow::setProgressValue(static_cast<int>(pos));
    }
}

void ConsoleProgressIndicator::Reset()
{
    myLastShownProgress = -1;
    std::cout << std::endl;
}


void ConsoleProgressIndicator::SetText(const Standard_CString theText)
{
    if (theText)
    {
        std::cout << "Current operation: " << theText << std::endl;
    }
}
