// Copyright (c) 2020 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

//------------------------------------------------------------------------------
int     show_rl_help(int, int);
int     show_rl_help_raw(int, int);

//------------------------------------------------------------------------------
int     clink_reload(int, int);
int     clink_reset_line(int, int);
int     clink_exit(int count, int invoking_key);
int     clink_ctrl_c(int count, int invoking_key);
int     clink_paste(int count, int invoking_key);
int     clink_copy_line(int count, int invoking_key);
int     clink_copy_word(int count, int invoking_key);
int     clink_copy_cwd(int count, int invoking_key);
int     clink_up_directory(int count, int invoking_key);
int     clink_insert_dot_dot(int count, int invoking_key);

//------------------------------------------------------------------------------
int     clink_scroll_line_up(int count, int invoking_key);
int     clink_scroll_line_down(int count, int invoking_key);
int     clink_scroll_page_up(int count, int invoking_key);
int     clink_scroll_page_down(int count, int invoking_key);
int     clink_scroll_top(int count, int invoking_key);
int     clink_scroll_bottom(int count, int invoking_key);

//------------------------------------------------------------------------------
int     clink_find_conhost(int count, int invoking_key);
int     clink_mark_conhost(int count, int invoking_key);

//------------------------------------------------------------------------------
int     clink_popup_directories(int count, int invoking_key);

//------------------------------------------------------------------------------
int     clink_complete_numbers(int count, int invoking_key);
int     clink_menu_complete_numbers(int count, int invoking_key);
int     clink_menu_complete_numbers_backward(int count, int invoking_key);
int     clink_old_menu_complete_numbers(int count, int invoking_key);
int     clink_old_menu_complete_numbers_backward(int count, int invoking_key);
int     clink_popup_complete_numbers(int count, int invoking_key);

//------------------------------------------------------------------------------
void    cua_clear_selection();
bool    cua_point_in_selection(int in);
int     cua_selection_event_hook(int event);
void    cua_after_command(bool force_clear=false);
int     cua_backward_char(int count, int invoking_key);
int     cua_forward_char(int count, int invoking_key);
int     cua_backward_word(int count, int invoking_key);
int     cua_forward_word(int count, int invoking_key);
int     cua_beg_of_line(int count, int invoking_key);
int     cua_end_of_line(int count, int invoking_key);
int     cua_copy(int count, int invoking_key);
int     cua_cut(int count, int invoking_key);
