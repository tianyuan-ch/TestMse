
#include "cm_log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

namespace cm {

	CLog::CLog(void)
		:unum_version_(0)
		,num_log_level_(kLOGNONE)
		,is_write_file_(false)
		,is_print_console_(true)
	{

	}

	void CLog::set_str_log_file_path(std::string str_log_file_path)
	{ 
		str_log_file_path_ = str_log_file_path; 
		FILE *fp;

		fp = fopen(str_log_file_path.c_str(), "wb");
		if(fp == NULL)
			return;

		time_t now;
		struct tm* time_now;
		time(&now);
		time_now = localtime(&now);
		fprintf(fp, "###################%04d,%02d,%02d version %d###################\n", time_now->tm_year, time_now->tm_mon, time_now->tm_mday, unum_version_);
		fclose(fp);
	}

	void CLog::Log(char *pcs_file_path, uint32_t unum_code_line, int num_level, const char *pcs_log, ...)
	{
		if(num_level > num_log_level_)
			return ;

		const char* pcs_log_level;
		switch (num_level)
		{
		case kLOGERROR:
			pcs_log_level = "error";
			break;
		case kLOGWARNING:
			pcs_log_level = "warning";
			break;
		case kLOGINFO:
			pcs_log_level = "info";
			break;
		case kLOGDEBUG:
			pcs_log_level = "debug";
			break;
		case kLOGFULL:
			pcs_log_level = "full";
			break;
		default:
			pcs_log_level = "unknown";
			break;
		}

		char cs_log[1024];
		va_list arg_list;
		va_start(arg_list, pcs_log);
		vsprintf(cs_log, pcs_log, arg_list);
		va_end(arg_list);

		char cs_text[1024];	

#ifdef WIN32
		char *pcs_file_name = strrchr(pcs_file_path, '\\');
#else
		char *pcs_file_name = pcs_file_path;
#endif

		if(is_write_file_)
		{

			time_t now;
			struct tm* time_now;
			time(&now);
			time_now = localtime(&now);
			sprintf(cs_text, "%s %02d:%02d:%02d [%s <line:%d>]%s\n", pcs_log_level, 
																	   time_now->tm_hour,
																	   time_now->tm_min,
																	   time_now->tm_sec,
																	   pcs_file_name, 
																	   unum_code_line, 
																	   cs_log);
			WriteLog(cs_text);
		}
		
		if(is_print_console_)
		{
			sprintf(cs_text, "%s [%s <line:%d>]%s\n", pcs_log_level, pcs_file_name, unum_code_line, cs_log);
			PrintLog(cs_text);
		}
	}
	void CLog::Log(int num_level, const char* pcs_log, ...)
	{
		if(num_level > num_log_level_)
			return ;

		const char* pcs_log_level;
		switch (num_level)
		{
		case kLOGERROR:
			pcs_log_level = "error";
			break;
		case kLOGWARNING:
			pcs_log_level = "warning";
			break;
		case kLOGINFO:
			pcs_log_level = "info";
			break;
		case kLOGDEBUG:
			pcs_log_level = "debug";
			break;
		case kLOGFULL:
			pcs_log_level = "full";
			break;
		default:
			pcs_log_level = "unknown";
			break;
		}

		char cs_log[1024];
		va_list arg_list;
		va_start(arg_list, pcs_log);
		vsprintf(cs_log, pcs_log, arg_list);
		va_end(arg_list);

		char cs_text[1024];	

		if(is_write_file_)
		{
			time_t now;
			struct tm* time_now;
			time(&now);
			time_now = localtime(&now);
			sprintf(cs_text, "%s %02d:%02d:%02d %s\n", pcs_log_level, 
													     time_now->tm_hour,
														 time_now->tm_min,
														 time_now->tm_sec,
														 cs_log);
			WriteLog(cs_text);
		}

		if(is_print_console_)
		{
			sprintf(cs_text, "log type:%s %s\n", pcs_log_level, cs_log);
			PrintLog(cs_text);
		}
	}


	void CLog::WriteLog(const char *pcs_log)
	{
		FILE *fp;

		fp = fopen(str_log_file_path_.c_str(), "a+");
		if(fp == NULL)
			return ;

		fprintf(fp, "%s", pcs_log);
		fclose(fp);

	}
	void CLog::PrintLog(const char *pcs_log)
	{
		printf("%s", pcs_log);
	}
}