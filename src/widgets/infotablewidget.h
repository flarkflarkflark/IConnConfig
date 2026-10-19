#ifndef INFOTABLEWIDGET_H
#define INFOTABLEWIDGET_H

#include "../sysex/getinfo.h"
#include "deviceinfowidget.h"

#include <QObject>
#include <QTableWidget>
#include <QWidget>

class InfoTableWidget : public QWidget {
	Q_OBJECT
  public:
	explicit InfoTableWidget(
		QWidget *parent,
		std::map<SysExMessage::DeviceInfoItem, std::shared_ptr<RetSetInfo>>
			*m_pRetSetInfos,
		bool readOnly = false);

  public slots:
	void onDeviceInfoChanged(int row, int column);

  signals:
	void deviceInfoChanged(SysExMessage::DeviceInfoItem item,
						   std::string value);

  private:
	void setupTable();
	void setUpTableItems();

  private:
	std::map<SysExMessage::DeviceInfoItem, std::shared_ptr<RetSetInfo>>
		*m_pRetSetInfos = nullptr;
	QTableWidget *m_pTableWidget = nullptr;
	bool m_bReadOnly = false;
};

#endif // INFOTABLEWIDGET_H
