#ifndef _CM_LOG_H_
#define _CM_LOG_H_

#include "cm_type.h"

#include <string>

namespace cm {

	enum ELOGLEVEL
	{
		kLOGNONE			= -1,
		kLOGERROR			= 0,
		kLOGWARNING			= 1,
		kLOGINFO			= 2,
		kLOGDEBUG			= 3,
		kLOGFULL			= 4
	};

	class CLog
	{
	public:
		CLog(void);
		~CLog(void) {};
		
		void set_unum_version(uint32_t unum_version){ unum_version_ = unum_version; }
		void set_str_log_file_path(std::string str_log_file_path);
		void set_is_write_file(bool is_write_file) { is_write_file_ = is_write_file;}
		void set_num_log_level(int num_log_level)  { num_log_level_ = num_log_level;}
		void set_is_print_console_(bool is_print_console) { is_print_console_ = is_print_console; }

		void Log(char *pcs_file_name, uint32_t unum_code_line, int num_level, const char *pcs_log, ...);
		void Log(int num_level, const char* pcs_log, ...);
	private:
		void WriteLog(const char *pcs_log);
		void PrintLog(const char *pcs_log);

	private:
		std::string str_log_file_path_;
		uint32_t unum_version_;
		int num_log_level_;
		bool is_write_file_;
		bool is_print_console_;
	};

}

#endif