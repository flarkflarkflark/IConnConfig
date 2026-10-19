#include "iconnconfigmain.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName("punkt-k");
	QCoreApplication::setOrganizationDomain("www.punkt-k.de");
	QCoreApplication::setApplicationName("iConnConfig");
	QCoreApplication::setApplicationVersion("0.5-beta");

	// Simple: just make everything bigger and readable
	app.setStyleSheet("* { font-size: 12pt; }");

	QTranslator mioConfigTranslator;
	QTranslator qtTranslator;
	QLocale::setDefault(QLocale("en_EN"));

	if (mioConfigTranslator.load(QLocale(), QLatin1String("iconnconfig"),
								 QLatin1String("_"),
								 QLatin1String(":/translations/tr"))) {
		app.installTranslator(&mioConfigTranslator);

		qtTranslator.load(
			QLocale(), QLatin1String("qt"), QLatin1String("_"),
			QLibraryInfo::location(QLibraryInfo::TranslationsPath));
		app.installTranslator(&qtTranslator);
	}

	QCommandLineParser *parser = new QCommandLineParser();
	parser->setApplicationDescription("iConnConfig - iConnectivity Device Configuration");
	parser->addHelpOption();
	parser->addVersionOption();

	QCommandLineOption filenameOption(
		QStringList() << "f" << "filename",
		QCoreApplication::translate("main", "filename to read settings from."),
		"filename");
	parser->addOption(filenameOption);
	QCommandLineOption dumpDiagnosticsOption(
		QStringList() << "dump-device-diagnostics",
		QCoreApplication::translate(
			"main",
			"Dump diagnostics for the selected device after opening the GUI."));
	parser->addOption(dumpDiagnosticsOption);
	QCommandLineOption dumpAudio4cClockOption(
		QStringList() << "dump-audio4c-clock",
		QCoreApplication::translate(
			"main",
			"Dump AUDIO4c clock investigation diagnostics and print a clock "
			"summary to stdout."));
	parser->addOption(dumpAudio4cClockOption);
	QCommandLineOption compareAudio4cClockOption(
		QStringList() << "compare-audio4c-clock",
		QCoreApplication::translate(
			"main",
			"Compare clock payloads across diagnostics files. Usage: "
			"--compare-audio4c-clock FILE1 FILE2 [FILE3...]"));
	parser->addOption(compareAudio4cClockOption);
	QCommandLineOption compareAudio4cGlobalOption(
		QStringList() << "compare-audio4c-global",
		QCoreApplication::translate(
			"main",
			"Compare global-audio payloads across diagnostics files. Usage: "
			"--compare-audio4c-global FILE1 FILE2 [FILE3...]"));
	parser->addOption(compareAudio4cGlobalOption);
	QCommandLineOption showReadonlyPanelsOption(
		QStringList() << "show-readonly-panels",
		QCoreApplication::translate(
			"main",
			"Show the MIDI/audio configuration panels of write-protected "
			"devices (AUDIO4c). Writes stay blocked."));
	parser->addOption(showReadonlyPanelsOption);
	parser->addPositionalArgument(
		"files",
		QCoreApplication::translate(
			"main",
			"Diagnostics files for --compare-audio4c-clock/--compare-audio4c-global "
			"mode."));

	parser->process(app);

	MioMain w(parser);
	w.show();
	return app.exec();
}