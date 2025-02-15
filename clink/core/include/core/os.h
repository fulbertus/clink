// Copyright (c) 2015 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#pragma once

#include <stdio.h>

class str_base;

//------------------------------------------------------------------------------
namespace os
{

enum {
    path_type_invalid,
    path_type_file,
    path_type_dir,
};

enum temp_file_mode {
    normal              = 0x0000,   // text mode (translate line endings)
    binary              = 0x0001,   // binary mode (no translation)
#if 0
    delete_on_close     = 0x0002,   // delete on close (requires FILE_SHARE_DELETE)
#endif
};

DEFINE_ENUM_FLAG_OPERATORS(temp_file_mode);

int     get_path_type(const char* path);
int     get_file_size(const char* path);
bool    is_hidden(const char* path);
void    get_current_dir(str_base& out);
bool    set_current_dir(const char* dir);
bool    make_dir(const char* dir);
bool    remove_dir(const char* dir);
bool    unlink(const char* path);
bool    move(const char* src_path, const char* dest_path);
bool    copy(const char* src_path, const char* dest_path);
bool    get_temp_dir(str_base& out);
FILE*   create_temp_file(str_base* out=nullptr, const char* prefix=nullptr, const char* ext=nullptr, temp_file_mode mode=normal, const char* path=nullptr);
bool    get_env(const char* name, str_base& out);
bool    set_env(const char* name, const char* value);
bool    get_alias(const char* name, str_base& out);
bool    get_short_path_name(const char* path, str_base& out);
bool    get_long_path_name(const char* path, str_base& out);
bool    get_full_path_name(const char* path, str_base& out);

}; // namespace os
