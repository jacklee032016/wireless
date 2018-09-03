#ifndef _BRCTL_H
#define _BRCTL_H
/*
 *
 */

struct command
{
	int		nargs;
	const char	*name;
	int		(*func)(int argc, char *const* argv);
	const char 	*help;
};

const struct command *command_lookup(const char *cmd);
void command_help(const struct command *);
void command_helpall(void);

void br_dump_bridge_id(const unsigned char *x);
void br_show_timer(const struct timeval *tv);
void br_dump_interface_list(const char *br);
void br_dump_info(const char *br, const struct bridge_info *bri);

#endif
