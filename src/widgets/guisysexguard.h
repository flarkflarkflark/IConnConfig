#ifndef GUISYSEXGUARD_H
#define GUISYSEXGUARD_H

#include "../iconnconfigmain.h"
#include "../sysex/communicationexception.h"
#include "../sysex/protocolexception.h"

#include <QObject>
#include <QStatusBar>
#include <QString>
#include <exception>
#include <iostream>

/**
 * Helpers for GUI code that executes SysEx commands from Qt slots. Exceptions
 * must never leave a slot (that terminates the application), so they are
 * logged, shown in the status bar and swallowed here.
 */
namespace GuiSysex {

inline void report(const QString &context, const QString &summary,
				   const std::string &details) {
	const QString text = QObject::tr("%1 failed: %2").arg(context, summary);
	std::cerr << context.toStdString() << " failed: " << details << std::endl;
	if (MioMain *mainWin = MioMain::getMainWin())
		mainWin->statusBar()->showMessage(text, 10000);
}

/** Runs fn(); returns false (after reporting) if it threw. */
template <typename Fn> bool run(const char *context, Fn &&fn) {
	const QString ctx = QString::fromUtf8(context);
	try {
		fn();
		return true;
	} catch (ProtocolException *e) {
		report(ctx,
			   QObject::tr("protocol error (code %1)")
				   .arg(static_cast<int>(e->getErrorCode())),
			   e->getErrorMessage());
		delete e;
	} catch (CommunicationException *e) {
		report(ctx,
			   QObject::tr("communication error (code %1)")
				   .arg(static_cast<int>(e->getErrorCode())),
			   e->getErrorMessage());
		delete e;
	} catch (ProtocolException &e) {
		report(ctx,
			   QObject::tr("protocol error (code %1)")
				   .arg(static_cast<int>(e.getErrorCode())),
			   e.getErrorMessage());
	} catch (CommunicationException &e) {
		report(ctx,
			   QObject::tr("communication error (code %1)")
				   .arg(static_cast<int>(e.getErrorCode())),
			   e.getErrorMessage());
	} catch (const std::exception &e) {
		report(ctx, QObject::tr("unexpected error"), e.what());
	}
	return false;
}

} // namespace GuiSysex

#endif // GUISYSEXGUARD_H
