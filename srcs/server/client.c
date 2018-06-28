/*
** EPITECH PROJECT, 2018
** add_remove
** File description:
** add_remove
*/

#include "server.h"

size_t count_row(char **array)
{
	size_t i = 0;

	while (array[i] != NULL)
		++i;
	return (i);
}

void send_to_all(t_irc *irc, t_session *session, char *msg, bool filter)
{
	t_session *client = NULL;

	for (t_node *c = irc->clients->head; c != NULL ; c = c->next) {
		client = (t_session *) c->data;
		if (filter == false || (filter == true && client->nickname
			&& session->nickname && (strcmp(session->nickname,
			client->nickname) != 0))) {
			write_fd(client->control.socket, msg);
		}
	}
}

void edit_channel(t_irc *irc, t_session *session, char *newnick)
{
	t_node *c = irc->channels->head;
	t_channel *channel = NULL;
	t_member *m = NULL;

	while (c != NULL) {
		channel = (t_channel *) c->data;
		if (strcmp(channel->name, session->nickname) == 0) {
			channel->name = free_secur(channel->name);
			channel->name = strdup(newnick);
		}
		m = get_by_member_name(session->nickname, channel->members);
		if (m) {
			m->nickname = free_secur(m->nickname);
			m->nickname = strdup(newnick);
		}
		c = c->next;
	}
}

int nick_function(t_irc *irc, t_session *s, char **tab)
{
	char *msg = NULL;
	t_session *other = NULL;

	if (tab[0] == NULL)
		return (dprintf(s->control.socket, ":%s 431 ERR_NONICK"
		"NAMEGIVEN :No nickname given%s", irc->control.ip, ENDL), 0);
	other = get_by_session_nickname(tab[0], irc->clients);
	if (other != NULL)
		return (dprintf(s->control.socket, ":%s 433 ERR_NICKNAMEINUSE "
		":%s is already in use%s", irc->control.ip, tab[0], ENDL), 0);
	if (s->nickname != NULL) {
		edit_channel(irc, s, tab[0]);
		asprintf(&msg, ":%s NICK %s%s", s->nickname, tab[0], ENDL);
		send_to_all(irc, s, msg, false);
		msg = free_secur(msg);
		s->nickname = free_secur(s->nickname);
	}
	s->nickname = strdup(tab[0]);
	s->connected = true;
	return (0);
}

int list_function(t_irc *irc, t_session *session, char **tab)
{
	t_node *c;
	t_channel *channel;
	char *target = tab[0] == NULL ? "\0" : tab[0];

	dprintf(session->control.socket, ":%s 321 RPL_LISTSTART Channel :User"
		"s Name%s", irc->control.ip, ENDL);
	if (irc->channels != NULL) {
		for (c = irc->channels->head; c != NULL ; c = c->next) {
			channel = (t_channel *) c->data;
			if (strstr(channel->name, target) != NULL)
			dprintf(session->control.socket, ":%s 322 RPL_LIST %s "
			":%s", irc->control.ip, channel->name, ENDL);
		}
	}
	dprintf(session->control.socket, ":%s 323 RPL_LISTEND :End of /L"
		"IST%s", irc->control.ip, ENDL);
	return (0);
}

int names_one_function(t_irc *irc, t_session *s, t_channel *channel)
{
	char *name = NULL;
	t_node *c = NULL;
	t_member *m = NULL;

	if (channel && channel->members != NULL) {
		dprintf(s->control.socket, ":%s 353 RPL_NAMREPLY %s%s",
			irc->control.ip, channel->name, ENDL);
		for (c = channel->members->head; c != NULL ; c = c->next) {
			m = (t_member *) c->data;
			if (m->nickname != NULL)
			dprintf(s->control.socket, ":%s%s", m->nickname, ENDL);
		}
	}
	name = (channel != NULL) ? channel->name : "";
	dprintf(s->control.socket, ":%s 366 RPL_ENDOFNAMES %s :End of /N"
		"AMES list%s", irc->control.ip, name, ENDL);
	return (0);
}

int names_function(t_irc *irc, t_session *session, char **tab)
{
	t_node *c = NULL;

	if (*tab != NULL)
		return names_one_function(irc, session,
			get_by_channel_by_name(*tab, irc->channels));
	for (c = irc->channels->head; c != NULL ; c = c->next)
		names_one_function(irc, session, (t_channel *)c->data);
	return (0);
}

int join_function(t_irc *irc, t_session *s, char **tab)
{
	int res = 0;
	char *channelname = NULL, *msg = NULL;

	if (tab[0] == NULL)
		return (dprintf(s->control.socket, ":%s 461 ERR_NEEDMOREPARAMS"
		" PART :Not enough parameters%s", irc->control.ip, ENDL), 0);
	channelname = get_by_channel_name(tab[0], s->channels);
	if (channelname == NULL)
		res = join_channel(irc, s, tab[0]);
	dprintf(s->control.socket, ":%s 331 RPL_NOTOPIC %s :%s",
		irc->control.ip, tab[0], ENDL);
	names_function(irc, s, tab);
	asprintf(&msg, ":%s JOIN :%s%s", s->nickname, tab[0], ENDL);
	send_to_all(irc, s, msg, false);
	free_secur(msg);
	return (res);
}

void send_to_channel(t_session *s, t_channel *channel, char *msg, bool filter)
{
	t_member *member = NULL;

	for (t_node *c = channel->members->head; c != NULL ; c = c->next) {
		member = (t_member *) c->data;
		if ((filter == false) || (filter == true &&
			(strcmp(member->nickname, s->nickname) != 0)))
			dprintf(member->fd, msg);
	}
}

int part_all_function(t_session *s, t_channel *c, int res)
{
	char *msg = NULL;

	asprintf(&msg, ":%s PART %s%s", s->nickname, c->name, ENDL);
	send_to_channel(s, c, msg, true);
	return (free_secur(msg), res);
}

int part_function(t_irc *i, t_session *s, char **tab)
{
	char *channelname = NULL;
	int res;
	t_channel *c = NULL;

	dprintf(s->control.socket, ":%s PART %s%s", s->nickname, tab[0], ENDL);
	if (tab[0] == NULL || *tab[0] == '\0')
		return (dprintf(s->control.socket, ":%s 461 ERR_NEEDMOREPARAMS"
		" PART :Not enough parameters%s", i->control.ip, ENDL), 0);
	channelname = get_by_channel_name(tab[0], s->channels);
	if (channelname == NULL)
	return (dprintf(s->control.socket, ":%s 442 ERR_NOTONCHANNEL "
	"%s :You're not on that channel%s", i->control.ip, tab[0], ENDL), 0);
	res = leave_channel(i, s, tab[0]);
	if (res == -1)
		return (dprintf(s->control.socket, ":%s 403 ERR_NOSUCHCHANNEL"
		" %s :No such channel%s", i->control.ip, tab[0], ENDL), 0);
	c = get_by_channel_by_name(tab[0], i->channels);
	if (!c)
		return (res);
	return (part_all_function(s, c, res));
}

int users_norm_function(t_irc *irc, t_session *session, bool empty)
{
	if (empty)
		dprintf(session->control.socket, ":%s 395 RPL_NOUSERS :Nobod"
			"y logged in%s", irc->control.ip, ENDL);
	return (dprintf(session->control.socket, ":%s 394 RPL_ENDOFUSERS :End "
			"of users%s", irc->control.ip, ENDL), 0);
}

int users_function(t_irc *irc, t_session *session, char **tab)
{
	bool empty = true;
	t_node *c;
	t_session *member;

	(void) tab;
	dprintf(session->control.socket, ":%s 392 RPL_USERSSTART :UserID Termi"
		"nal Hôte%s", irc->control.ip, ENDL);
	if (irc->clients != NULL)
		for (c = irc->clients->head; c != NULL ; c = c->next) {
			member = (t_session *) c->data;
			if (member->nickname != NULL) {
			empty = false;
			dprintf(session->control.socket, ":%s 393 RPL_USERS :"
			"%s%s", irc->control.ip, member->nickname, ENDL);
			}
		}
	return (users_norm_function(irc, session, empty));
}

int write_to_channel(t_irc *irc, t_session *s, char **tab)
{
	char *msg = NULL;
	t_channel *channel = NULL;

	if (count(s->channels) == 0)
		return (dprintf(s->control.socket, ":%s 401 ERR_NOSUCHNICK :No"
			" such channel%s", irc->control.ip, ENDL), 0);
	channel = get_by_channel_by_name(tab[0], irc->channels);
	if (channel == NULL)
		return (dprintf(s->control.socket, ":%s 401 ERR_NOSUCHNICK %s"
		" :No such channel%s", irc->control.ip, &tab[0][1], ENDL), 0);
	asprintf(&msg, ":%s PRIVMSG %s", s->nickname, channel->name);
	for (size_t i = 1 ; tab[i] != NULL ; ++i)
		asprintf(&msg, "%s %s", msg, tab[i]);
	asprintf(&msg, "%s%s", msg, ENDL);
	send_to_channel(s, channel, msg, true);
	free(msg);
	return (0);
}

int send_privmsg(t_irc *irc, t_session *s, char **tab)
{
	char *target_name = NULL, *msg = NULL;
	t_session *target = NULL;

	target_name = tab[0];
	if (tab[1] == NULL)
		return (dprintf(s->control.socket, ":%s 412 ERR_NOTEXTTOSEND"
			" :No text to send%s", irc->control.ip, ENDL), 0);
	target = get_by_session_nickname(target_name, irc->clients);
	if (target == NULL)
	return (dprintf(s->control.socket, ":%s 401 ERR_NOSUCHNICK %s "
	":No such nick/channel%s", irc->control.ip, target_name, ENDL), 0);
	asprintf(&msg, ":%s PRIVMSG %s", s->nickname, target_name);
	for (size_t i = 1; tab[i] != NULL ; ++i)
		asprintf(&msg, "%s %s", msg, tab[i]);
	asprintf(&msg, "%s%s", msg, ENDL);
	fprintf(stderr, "MESSAGE PRIVE:%s", msg);
	write_fd(target->control.socket, msg);
	free(msg);
	return (0);
}

int send_msg(t_irc *irc, t_session *session, char **tab)
{
	if (tab[0][0] == '#')
		return (write_to_channel(irc, session, tab));
	return (send_privmsg(irc, session, tab));
}

int send_privfile(t_irc *irc, t_session *session, char **tab)
{
	(void) irc;
	(void) session;
	(void) tab;
	fprintf(stderr, "Envoie de fichier non géré\n");
	return (0);
}

int privmsg_function(t_irc *irc, t_session *session, char **tab)
{
	size_t argu = count_row(tab);

	if (argu == 0)
		return (dprintf(session->control.socket,
		":%s 411 ERR_NORECIPIENT :No recipient given (PRIVMSG)%s",
		irc->control.ip, ENDL), 0);
	else if (argu >= 2 && *tab[1] == ':')
		return (send_msg(irc, session, tab));
	else if (argu == 3)
		return (send_privfile(irc, session, tab));
	return (fprintf(stderr, "Bad format.\n"), 0);
}

int user_function(t_irc *irc, t_session *session, char **buffer)
{
	if (count_row(buffer) < 3)
		return (dprintf(session->control.socket,
		":%s 461 ERR_NEEDMOREPARAMS USER :Not enough parameters%s",
		irc->control.ip, ENDL));
	session->username = strdup(buffer[0]);
	session->host = strdup(buffer[1]);
	session->server = strdup(buffer[2]);
	session->realname = strdup(buffer[3]);
	dprintf(session->control.socket,
		":%s 001 %s :Welcome to the IRC Network %s!%s@%s%s",
		irc->control.ip,
		session->nickname,
		session->nickname,
		session->username,
		session->control.ip,
		ENDL);
	return (0);
}

int quit_function(t_irc *irc, t_session *session, char **tab)
{
	(void) tab;
	remove_client(irc, session);
	return (1);
}


int pong_function(t_irc *irc, t_session *session, char **tab)
{
	(void) tab;
	(void) irc;
	session->ping.enable = false;
	return (0);
}

int ping_function(t_irc *irc, t_session *session, char **tab)
{
	(void) tab;
	(void) irc;
	dprintf(session->control.socket, ":%s 000 PONG %s%s",
		irc->control.ip, tab[0], ENDL);
	return (0);
}

t_server_pointer_cmd *get_command(void)
{
	static t_server_pointer_cmd c[] = {
			{"NICK", strlen("NICK"), nick_function, false},
			{"USER", strlen("USER"), user_function, false},
			{"LIST", strlen("LIST"), list_function, true},
			{"JOIN", strlen("JOIN"), join_function, true},
			{"PART", strlen("PART"), part_function, true},
			{"USERS", strlen("USERS"), users_function, true},
			{"NAMES", strlen("NAMES"), names_function, true},
			{"PRIVMSG", strlen("PRIVMSG"), privmsg_function, true},
			{"PONG", strlen("PONG"), pong_function, false},
			{"PING", strlen("PING"), ping_function, false},
			{"QUIT", strlen("QUIT"), quit_function, false}};
	return (c);
}

int manage_command(t_irc *irc, t_session *session, char **tab)
{
	const size_t nb = 11;
	t_server_pointer_cmd *c = get_command();

	session->ping.time = time(NULL);
	for (size_t i = 0 ; i < nb ; ++i)
		if (my_strncmp(tab[0], c[i].name, c[i].size) == 0) {
			if (c[i].required_connecion == true &&
			session->connected == false)
			return (dprintf(session->control.socket,
			"300 RPL_NONE%s", ENDL), 0);
			return (c[i].ptr_function(irc, session, &tab[1]));
		}
	return (dprintf(session->control.socket, "300 RPL_NONE%s", ENDL), 0);
}

int manage_client(t_irc *irc, t_session *session)
{
	char *buffer = NULL, *cmd = NULL, **array = NULL;

	if (!is_valid_fd(session->control.socket))
		return (remove_client(irc, session), 1);
	buffer = read_fd(session->control.socket);
	if (buffer == NULL)
		return (remove_client(irc, session), 1);
	cmd = strtok(buffer, ENDL);
	while (cmd && !my_break(false)) {
		printf("CMD:%s\n", cmd);
		array = str_to_tab(cmd);
		if (array == NULL)
			return (84);
		if (manage_command(irc, session, array) == 1)
			return (1);
		free_array(array);
		cmd = strtok(NULL, ENDL);
	}
	return (0);
}
