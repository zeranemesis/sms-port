
class TBombHei {
public:
	void setWalkAnm();
	void setAfterDeadEffect();
	bool doKeepDistance();
	const char** getBasNameTable() const;
};

static const char* bombhei_bastable[] = {
	"/scene/bombhei/bas/downnejibomb_down1.bas",
	nullptr,
	nullptr,
	"/scene/bombhei/bas/nejibomb_land1.bas",
	nullptr,
	nullptr,
	"/scene/bombhei/bas/nejibomb_stop_down1.bas",
};

const char** TBombHei::getBasNameTable() const { return bombhei_bastable; }

bool TBombHei::doKeepDistance()
{
	return *(unsigned char*)((char*)this + 0x19C);
}

void TBombHei::setWalkAnm()
{
	typedef void (*SetBckAnm)(TBombHei*, int);
	reinterpret_cast<SetBckAnm>((*reinterpret_cast<void***>(this))[100])(this,
	                                                                    4);
}

void TBombHei::setAfterDeadEffect() { }
