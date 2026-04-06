#pragma once

#include <cstdio> 
#include "./messages.h"

bool parse_add_message(FILE* fp, message* out); 

bool parse_cancel_message(FILE* fp, message* out);

bool parse_modify_message(FILE* fp, message* out);

bool parse_trade_message(FILE* fp, message* out);

bool parse_next_message(FILE* fp,  message* out);


