#ifndef GOTILINE_DLG_H
#define GOTILINE_DLG_H

#include "DockingDlgInterface.h"
#include "resource.h"

class IsaViewerDlg : public DockingDlgInterface
{
public :
	IsaViewerDlg() : DockingDlgInterface(IDD_ISA_VIEWER){};

    virtual void display(bool toShow = true) const
    {
        DockingDlgInterface::display(toShow);
    };

	void setParent(HWND parent2set)
    {
		_hParent = parent2set;
	};

protected :
	virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

private :
    void saveConfig();
    void loadConfig();
    std::string compile();
};

#endif //GOTILINE_DLG_H
