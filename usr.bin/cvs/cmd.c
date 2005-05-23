/*	$OpenBSD: cmd.c,v 1.18 2005/05/23 17:43:54 xsa Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "rcs.h"
#include "proto.h"

/*
 * start the execution of a command.
 */
int
cvs_startcmd(struct cvs_cmd *cmd, int argc, char **argv)
{
	int i;
	int ret;
	struct cvsroot *root;
	struct cvs_cmd_info *c = cmd->cmd_info;

	/* if the command requested is the server one, just call the
	 * cvs_server() function to handle it, and return after it.
	 */
	if (cmd->cmd_op == CVS_OP_SERVER) {
		ret = cvs_server(argc, argv);
		return (ret);
	}

	if (c->cmd_options != NULL) {
		if ((ret = c->cmd_options(cmd->cmd_opts, argc, argv, &i)) != 0)
			return (ret);

		argc -= i;
		argv += i;
	}

	if ((c->cmd_helper != NULL) && ((ret = c->cmd_helper()) != 0))
		return (ret);

	if ((root = cvsroot_get(".")) == NULL)
		return (CVS_EX_BADROOT);

	if (cvs_trace)
		cvs_log(LP_TRACE, "cvs_startcmd() CVSROOT=%s", root->cr_str);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_connect(root) < 0)
			return (CVS_EX_PROTO);

		if (c->cmd_flags & CVS_CMD_SENDARGS1) {
			for (i = 0; i < argc; i++) {
				if (cvs_sendarg(root, argv[i], 0) < 0)
					return (CVS_EX_PROTO);
			}
		}

		if (c->cmd_sendflags != NULL) {
			if ((ret = c->cmd_sendflags(root)) != 0)
				return (ret);
		}

		if (c->cmd_flags & CVS_CMD_NEEDLOG) {
			if (cvs_logmsg_send(root, cvs_msg) < 0)
				return (CVS_EX_PROTO);
		}
	}

	/* if we are the version command, don't bother going
	 * any further now, we did everything we had to.
	 */
	if (cmd->cmd_op == CVS_OP_VERSION)
		return (0);

	if ((c->cmd_flags & CVS_CMD_ALLOWSPEC) && argc != 0) {
		cvs_files = cvs_file_getspec(argv, argc, c->file_flags,
		    c->cmd_examine, NULL);
	} else {
		cvs_files = cvs_file_get(".", c->file_flags,
		    c->cmd_examine, NULL);
	}

	if (cvs_files == NULL)
		return (CVS_EX_DATA);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (c->cmd_flags & CVS_CMD_SENDDIR) {
			if (cvs_senddir(root, cvs_files) < 0)
				return (CVS_EX_PROTO);
		}

		if (c->cmd_flags & CVS_CMD_SENDARGS2) {
			for (i = 0; i < argc; i++) {
				if (cvs_sendarg(root, argv[i], 0) < 0)
					return (CVS_EX_PROTO);
			}
		}

		if (cvs_sendreq(root, c->cmd_req,
		    (cmd->cmd_op == CVS_OP_INIT) ? root->cr_dir : NULL) < 0)
			return (CVS_EX_PROTO);
	}

	return (0);
}
