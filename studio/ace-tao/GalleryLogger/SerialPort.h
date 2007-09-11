// SerialPort.h

#include <afxwin.h>         // MFC core and standard components

class SerialPort
{
public:
	SerialPort();
	~SerialPort();
	void Open(const char * port);
	int Write(const char * data, int size);
	int Read(char * buf, int size);
private:
	HANDLE mHandle;
};

