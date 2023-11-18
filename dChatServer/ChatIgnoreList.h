#ifndef __CHATIGNORELIST__H__
#define __CHATIGNORELIST__H__

struct Packet;

namespace ChatIgnoreList {
	void GetIgnoreList(Packet* packet);
	void AddIgnore(Packet* packet);
};

#endif  //!__CHATIGNORELIST__H__
