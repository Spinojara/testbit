#define _XOPEN_SOURCE 500
#include "single.h"

#include <stdlib.h>

#include "tui.h"
#include "color.h"
#include "draw.h"
#include "req.h"
#include "con.h"
#include "oldtest.h"
#include "sprt.h"
#include "util.h"
#include "active.h"
#include "binary.h"

extern const int refresh_interval;

void draw_table(struct oldteststate *os);
void draw_patch(struct oldteststate *os, int lazy, int up);
int load_single(struct oldteststate *os);
int load_patch(struct oldteststate *os);

void free_single(struct oldteststate *os) {
	if (os->patch)
		free_file(os->patch);

	os->patch = os->top = NULL;
}

void handle_single(struct oldteststate *os, chtype ch) {
	int update = 0;
	switch (ch) {
	case KEY_ESC:
		os->single = 0;
		os->last_loaded = 0;
		free_single(os);
		draw_oldtest(os, 0, 1);
		break;
	case 'r':
		os->last_loaded = 0;
		update = 1;
		break;
	case KEY_UP:
	case 'k':
		if (os->top && os->top->prev) {
			os->top = os->top->prev;
			draw_patch(os, 1, 0);
			wrefresh(os->win);
		}
		break;
	case KEY_DOWN:
	case 'j':
		if (os->top && os->top->next && os->fills) {
			os->top = os->top->next;
			draw_patch(os, 1, 1);
			wrefresh(os->win);
		}
		break;
	default:
		break;
	}

	if (update)
		draw_single(os, 1, 1);
}

void draw_single(struct oldteststate *os, int lazy, int load) {
	if (!os->patch)
		load_patch(os);

	if (load && (time(NULL) - os->last_loaded > refresh_interval)) {
		load_single(os);
		lazy = 0;
	}

	if (!lazy)
		draw_border(os->win, NULL, &cs.bordershadow, &cs.border, 1, 0, 0, getmaxy(os->win), getmaxx(os->win));

	draw_table(os);

	draw_patch(os, lazy, 0);

	wrefresh(os->win);
}

int load_single(struct oldteststate *os) {
	sendf(os->ssl, "ccq", REQUESTLOGTEST, OLDTESTSINGLE, os->selected_id);
	char response;
	recvf(os->ssl, "c", &response);
	if (response)
		return 1;
	return recvf(os->ssl, "ccDDDDDDDDDDssqqqLLLLLLLL",
			&os->singletest.type, &os->singletest.status,
			&os->singletest.maintime, &os->singletest.increment,
			&os->singletest.alpha, &os->singletest.beta,
			&os->singletest.llr, &os->singletest.elo0,
			&os->singletest.elo1, &os->singletest.eloe,
			&os->singletest.elo, &os->singletest.pm,
			os->singletest.branch, sizeof(os->singletest.branch),
			os->singletest.commit, sizeof(os->singletest.commit),
			&os->singletest.qtime, &os->singletest.stime,
			&os->singletest.dtime, &os->singletest.t0,
			&os->singletest.t1, &os->singletest.t2,
			&os->singletest.p0, &os->singletest.p1,
			&os->singletest.p2, &os->singletest.p3,
			&os->singletest.p4);
}

int load_patch(struct oldteststate *os) {
	sendf(os->ssl, "cq", REQUESTPATCH, os->selected_id);
	char response;
	recvf(os->ssl, "c", &response);
	if (response)
		return 1;
	
	char *patch = recvstrdynamic(os->ssl, 16 * 1024 * 1024);
	if (!patch)
		return 1;

	os->top = os->patch = new_file(patch);
	free(patch);

	return 0;
}

static void attr(const struct oldteststate *os, int i, int j) {
	if (i != 1 || j != 0)
		return;

	switch (os->singletest.status) {
	case TESTQUEUE:
	case TESTCANCEL:
	case TESTINCONCLUSIVE:
		wattrset(os->win, cs.yellow.attr);
		break;
	case TESTERRBRANCH:
	case TESTERRCOMMIT:
	case TESTERRPATCH:
	case TESTERRMAKE:
	case TESTERRRUN:
	case TESTH0:
		wattrset(os->win, cs.error.attr);
		break;
	case TESTRUN:
	case TESTH1:
	case TESTELO:
		wattrset(os->win, cs.green.attr);
		break;
	}
}

void draw_table(struct oldteststate *os) {
	os->tests = 1;
	struct test *test = &os->singletest;

	char status[2][128];
	char tc[2][128];
	char elo[2][128];
	char llr[2][128];
	char ab[2][128];
	char elo0[2][128];
	char elo1[2][128];
	char eloe[2][128];
	char tri[2][128];
	char penta[2][128];
	char qtime[2][128];
	char stime[2][128];
	char dtime[2][128];
	char branch[2][128];
	char commit[2][128];

	double A = log(test->beta / (1.0 - test->alpha));
	double B = log((1.0 - test->beta) / test->alpha);

	sprintf(status[0], "Status");
	sprintf(tc[0], "TC");
	sprintf(elo[0], "Elo");
	sprintf(llr[0], "LLR");
	sprintf(ab[0], "(A, B)");
	sprintf(elo0[0], "Elo0");
	sprintf(elo1[0], "Elo1");
	sprintf(eloe[0], "EloE");
	sprintf(tri[0], "Trinomial");
	sprintf(penta[0], "Pentanomial");
	sprintf(qtime[0], "Queue Timestamp");
	sprintf(stime[0], "Start Timestamp");
	sprintf(dtime[0], "Done Timestamp");
	sprintf(branch[0], "Branch");
	sprintf(commit[0], "Commit");

	int started = test->t0 + test->t1 + test->t2 > 0;
	char tmp1[128], tmp2[128];
	iso8601local(qtime[1], test->qtime);
	iso8601local(stime[1], test->stime);
	iso8601local(dtime[1], test->dtime);
	sprintf(tc[1], "%s+%s", fstr(tmp1, test->maintime, 2), fstr(tmp2, test->increment, 2));
	if (started) {
		sprintf(elo[1], "%.2lf+%.2lf", test->elo, test->pm);
		sprintf(llr[1], "%.2lf", test->llr);
	}
	else {
		sprintf(elo[1], "N/A");
		sprintf(llr[1], "N/A");
	}
	sprintf(tri[1], "%u-%u-%u", test->t0, test->t1, test->t2);
	sprintf(penta[1], "%u-%u-%u-%u-%u", test->p0, test->p1, test->p2, test->p3, test->p4);
	sprintf(ab[1], "(%.2lf, %.2lf)", A, B);
	sprintf(elo0[1], "%.1lf", test->elo0);
	sprintf(elo1[1], "%.1lf", test->elo1);
	sprintf(eloe[1], "%.1lf", test->eloe);
	strcpy(branch[1], test->branch);
	strcpy(commit[1], test->commit);

	switch (test->status) {
	case TESTQUEUE:
		sprintf(status[1], "Pending");
		break;
	case TESTRUN:
		sprintf(status[1], "Running");
		break;
	case TESTCANCEL:
		sprintf(status[1], "Cancelled");
		break;
	case TESTERRBRANCH:
		sprintf(status[1], "Branch not found");
		break;
	case TESTERRCOMMIT:
		sprintf(status[1], "Commit not found");
		break;
	case TESTERRPATCH:
		sprintf(status[1], "Failed to apply patch");
		break;
	case TESTERRMAKE:
		sprintf(status[1], "Failed to compile");
		break;
	case TESTERRRUN:
		sprintf(status[1], "Runtime error");
		break;
	case TESTINCONCLUSIVE:
		sprintf(status[1], "Inconclusive");
		break;
	case TESTH0:
		sprintf(status[1], "H0 accepted");
		break;
	case TESTH1:
		sprintf(status[1], "H1 accepted");
		break;
	case TESTELO:
		sprintf(status[1], "Done");
		break;
	}
	
	switch (test->status) {
	case TESTQUEUE:
		if (test->type == TESTTYPESPRT)
			draw_dynamic(os, &attr, 0, status, tc, ab, elo0, elo1, qtime, branch, commit, NULL);
		else
			draw_dynamic(os, &attr, 0, status, tc, eloe, qtime, branch, commit, NULL);
		break;
	case TESTRUN:
		sprintf(dtime[0], "ETA");
		if (started)
			etastr(dtime[1], test, A, B);
		else
			sprintf(dtime[1], "N/A");
		/* fallthrough */
	case TESTCANCEL:
	case TESTERRRUN:
		if (test->type == TESTTYPESPRT)
			draw_dynamic(os, &attr, 0, status, tc, elo, tri, penta, llr, ab, elo0, elo1, dtime, stime, qtime, branch, commit, NULL);
		else
			draw_dynamic(os, &attr, 0, status, tc, elo, tri, penta, eloe, dtime, stime, qtime, branch, commit, NULL);
		break;
	case TESTERRBRANCH:
	case TESTERRCOMMIT:
	case TESTERRPATCH:
	case TESTERRMAKE:
		if (test->type == TESTTYPESPRT)
			draw_dynamic(os, &attr, 0, status, tc, ab, elo0, elo1, dtime, stime, qtime, branch, commit, NULL);
		else
			draw_dynamic(os, &attr, 0, status, tc, eloe, dtime, stime, qtime, branch, commit, NULL);
		break;
	case TESTINCONCLUSIVE:
	case TESTH0:
	case TESTH1:
		draw_dynamic(os, &attr, 0, status, tc, elo, tri, penta, llr, ab, elo0, elo1, dtime, stime, qtime, branch, commit, NULL);
		break;
	case TESTELO:
		draw_dynamic(os, &attr, 0, status, tc, elo, tri, penta, eloe, dtime, stime, qtime, branch, commit, NULL);
		break;
	}
}

void draw_patch(struct oldteststate *os, int lazy, int up) {

	int min_y = 12;
	int min_x = 3;
	int max_y = getmaxy(os->win) - 3;
	int max_x = getmaxx(os->win) - 4;
	int size_x = max_x - min_x + 1;
	int size_y = max_y - min_y + 1;

	if (!lazy) {
		wattrset(os->win, cs.textdim.attr);
		mvwhline(os->win, min_y - 1, min_x, 0, size_x);
		mvwhline(os->win, max_y + 1, min_x, 0, size_x);
		mvwvline(os->win, min_y, min_x - 1, 0, size_y);
		mvwvline(os->win, min_y, max_x + 1, 0, size_y);
		mvwaddch(os->win, min_y - 1, min_x - 1, ACS_ULCORNER);
		mvwaddch(os->win, max_y + 1, min_x - 1, ACS_LLCORNER);
		mvwaddch(os->win, min_y - 1, max_x + 1, ACS_URCORNER);
		mvwaddch(os->win, max_y + 1, max_x + 1, ACS_LRCORNER);
	}

	wattrset(os->win, cs.text.attr);

	struct line *cur;
	int y = min_y;
	for (cur = os->top; cur && y <= max_y; cur = cur->next, y++) {
		if (lazy) {
			size_t prev;
			if (up)
				prev = cur->prev ? cur->prev->len : 0;
			else
				prev = cur->next ? cur->next->len : 0;
			if ((size_t)size_x > cur->len && prev > cur->len) {
				size_t len = prev - cur->len;
				int left = size_x - cur->len;
				int printlen = len > (size_t) left ? left : (int)len;
				mvwhline(os->win, y, min_x + cur->len, ' ', printlen);
			}
#ifdef WINDOWS_TERMINAL_BUG
			wrefresh(os->win);
#endif
		}
		mvwaddnstrtab(os->win, y, min_x, cur->data, size_x);
#ifdef WINDOWS_TERMINAL_BUG
		wrefresh(os->win);
#endif
	}
	os->fills = cur != NULL;
}
