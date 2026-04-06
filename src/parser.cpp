#include "./messages.h" 
#include <cstdint>
#include <cstdio>

bool parse_add_message(FILE* fp, message* out) {
    return fread(&out->add, 1, sizeof(add_order_msg), fp) == sizeof(add_order_msg);    
} 

bool parse_cancel_message(FILE* fp, message* out) {
    return fread(&out->cancel, 1, sizeof(cancel_order_msg), fp) == sizeof(cancel_order_msg);    
}

bool parse_modify_message(FILE* fp, message* out) {
    return fread(&out->modify, 1, sizeof(modify_order_msg), fp) == sizeof(modify_order_msg);    
}

bool parse_trade_message(FILE* fp, message* out) {
    return fread(&out->trade, 1, sizeof(trade_msg), fp) == sizeof(trade_msg);    
}

bool parse_next_message(FILE* fp,  message* out) {

    if (fread(&out->type, 1, 1, fp) != 1) return false;

    switch(out->type) {
        case ADD: {return parse_add_message(fp, out); break; }
        case CANCEL: {return parse_cancel_message(fp, out); break; }
        case MODIFY: {return parse_modify_message(fp, out); break; }
        case TRADE: {return parse_trade_message(fp, out); break; }
    }

    return false;

    
}

