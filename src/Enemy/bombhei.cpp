
class TBombHei {
public:
	void setWalkAnm();
};

void TBombHei::setWalkAnm()
{
	typedef void (*SetBckAnm)(TBombHei*, int);
	reinterpret_cast<SetBckAnm>((*reinterpret_cast<void***>(this))[100])(this,
	                                                                    4);
}
