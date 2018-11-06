#ifndef _CM_STRING_CODING_H_
#define _CM_STRING_CODING_H_

#include <string>

namespace cm{

	int cm_string_gb2312_to_utf8(char *text, int len, std::string& out);


}


#endif