#pragma once

#include <iostream>
#include <Message_ProgressIndicator.hxx>
#include <Message_ProgressRange.hxx>
#include <Message_ProgressScope.hxx>
#include <Standard_CString.hxx>
#include <Standard_Handle.hxx>
#include <Standard_Type.hxx>

class ConsoleProgressIndicator;
DEFINE_STANDARD_HANDLE(ConsoleProgressIndicator, Message_ProgressIndicator)

class ConsoleProgressIndicator : public Message_ProgressIndicator
{
public:
	ConsoleProgressIndicator();

	// OCCT RTTI
	DEFINE_STANDARD_RTTIEXT(ConsoleProgressIndicator, Message_ProgressIndicator)

	// Overridden methods
	void Show(const Message_ProgressScope& theScope, const Standard_Boolean theForce) override;

	void Reset() override;

	// Optional: Override SetText to display scope names
	void SetText(const Standard_CString theText);

private:
	Standard_Real myLastShownProgress;
};
