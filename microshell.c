#include <stdlib.h>
#include <unistd.h>
#include <string.h>  

#define TYPE_END	0
#define TYPE_PIPE	1
#define TYPE_BREAK	2

typedef struct s_list
{
    char    **args;
    int     length;
    int     type;
    int     pipe[2];
    struct s_list   *previous;
    struct s_list   *next;
}   t_list;

int ft_strlen(char *s)
{
    int i = 0;

    if (!s)
        return (0);
    while (s[i])
        i++;
    return (i);
}

void    put_error(char *s)
{
    write(2, s, ft_strlen(s));
}

void    exit_fatal(void)
{
    put_error("error: fatal\n");
    exit(1);
}

char	*ft_strdup(char *s)
{
	char	*copy;
	int	i;

	if(!(copy = (char *)malloc(ft_strlen(s) + 1)))
        exit_fatal();
	i = -1;
	while (s[++i])
		copy[i] = s[i];
	copy[i] = 0;
	return (copy);
}

int add_arg(t_list *cmd, char *arg)
{
    char    **tmp = NULL;
    int     i = 0;

    if (!(tmp = (char **)malloc(sizeof(*tmp) * (cmd->length + 2))))
        exit_fatal();
    while (i < cmd->length)
    {
        tmp[i] = cmd->args[i];
        i++;
    }
    if (cmd->length > 0)
        free(cmd->args);
    cmd->args = tmp;
    cmd->args[i++] = ft_strdup(arg);
    cmd->args[i] = 0;
    cmd->length++;
    return (0);
}

int list_push(t_list **cmds, char *arg)
{
    t_list  *new;

    if (!(new = (t_list *)malloc(sizeof(*new))))
        exit_fatal();
    new->args = NULL;
    new->length = 0;
    new->type = TYPE_END;
    new->previous = NULL;
    new->next = NULL;
    if (*cmds)
    {
        (*cmds)->next = new;
        new->previous = *cmds;
    }
    *cmds = new;
    return(add_arg(new, arg));
}

void list_clear(t_list **cmds)
{
    t_list  *tmp;
    int     i;

    while (*cmds && (*cmds)->previous)
        *cmds = (*cmds)->previous;
    while (*cmds)
    {
        tmp = (*cmds)->next;
        i = 0;
        while (i < (*cmds)->length)
            free((*cmds)->args[i++]);
        free((*cmds)->args);
        free(*cmds);
        *cmds = tmp;
    }
    *cmds = NULL;
}

int    parse(t_list **cmds, char *arg)
{
    int isbreak = (strcmp(";", arg) == 0);

    if (isbreak && !*cmds)
        return (0);
    if (!isbreak && (!*cmds || (*cmds)->type > TYPE_END))
        return (list_push(cmds, arg));
    if (strcmp("|", arg) == 0)
        (*cmds)->type = TYPE_PIPE;
    else if (isbreak)
        (*cmds)->type = TYPE_BREAK;
    else
        return (add_arg(*cmds, arg));
    return (0);
}

int exec_cmd(t_list *cmd, char **env)
{
    pid_t   pid;
    int     exits = 1;
    int     status;
    int     pipe_open = 0;

    if (cmd->type == TYPE_PIPE || (cmd->previous && cmd->previous->type == TYPE_PIPE))
    {
        pipe_open = 1;
        if (pipe(cmd->pipe))
            exit_fatal();
    }
    pid = fork();
    if (pid < 0)
        exit_fatal();
    if (pid == 0)
    {
        if (cmd->type == TYPE_PIPE && dup2(cmd->pipe[1], 1) < 0)
            exit_fatal();
        if (cmd->previous && cmd->previous->type == TYPE_PIPE && dup2(cmd->previous->pipe[0], 0) < 0)
            exit_fatal();
        if ((exits = execve(cmd->args[0], cmd->args, env)) < 0)
        {
            put_error("error: cannot execute ");
            put_error(cmd->args[0]);
            put_error("\n");
        }
        exit(exits);
    }
    else
    {
        waitpid(pid, &status, 0);
        if (pipe_open)
        {
            close(cmd->pipe[1]);
            if (!cmd->next || cmd->type == TYPE_BREAK)
                close(cmd->pipe[0]);
        }
        if (cmd->previous && cmd->previous->type == TYPE_PIPE)
            close(cmd->previous->pipe[0]);
        if (WIFEXITED(status))
            exits = WEXITSTATUS(status);
    }
    return (exits);
}

int exec_cmds(t_list **cmds, char **env)
{
    int exit = 0;

    while (*cmds && (*cmds)->previous)
        *cmds = (*cmds)->previous;
    while (*cmds)
    {
        if (strcmp("cd", (*cmds)->args[0]) == 0)
        {
            exit = 0;
            if ((*cmds)->length < 2)
            {
                put_error("error: cd: bad arguments\n");
                exit = 1;
            }
            else if (chdir((*cmds)->args[1]))
            {
                exit = 1;
                put_error("error: cd: cannot change directory to ");
                put_error((*cmds)->args[1]);
                put_error("\n");
            }
        }
        else
            exit = exec_cmd(*cmds, env);
        *cmds = (*cmds)->next;
    }
    return (exit);
}

int main(int ac, char **av, char **env)
{
    t_list  *cmds;
    int i;
    int ret;

    cmds = NULL;
    i = 1;
    while (i < ac)
        parse(&cmds, av[i++]);
    if (cmds)
        ret = exec_cmds(&cmds, env);
    list_clear(&cmds);
    return (ret);
}