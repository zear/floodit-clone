#include <time.h>
#include "game.h"
#include "utils.h"
#include "high_score.h"
#include "multiplayer.h"

char _spreadColor(s_Game *game, int selectedColor, int startX, int startY);
void _generateGrid(s_Game* game);
void _generateFirstPlayer(s_Game *game);
void _notifyServerPlayerTurn(s_Game *game);
void _processServerPackets(s_Game *game);
char _processClientPackets(s_Game *game);
void _setRotatedGridPacket(s_Game *game, s_TCPpacket *packet, int playerIndex);

int g_startPositionPlayers[4][2] = {
	{0, 0},
	{WIDTH_GRID - 1, HEIGHT_GRID - 1},
	{WIDTH_GRID - 1, 0},
	{0, HEIGHT_GRID - 1}
};

void game_init(s_Game *game) {
	game->scoreFont = TTF_OpenFont("ClearSans-Medium.ttf", 18);
	game->endFont = TTF_OpenFont("ClearSans-Medium.ttf", 18);
	game->menuFont = TTF_OpenFont("ClearSans-Medium.ttf", 18);
	game->selectedMenuFont = TTF_OpenFont("ClearSans-Medium.ttf", 24);
	game->highScoreTitleFont = TTF_OpenFont("ClearSans-Medium.ttf", 24);
	game->highScoreFont = TTF_OpenFont("ClearSans-Medium.ttf", 18);
	game->colors[0][0] = 255;
	game->colors[0][1] = 0;
	game->colors[0][2] = 0;
	game->colors[1][0] = 0;
	game->colors[1][1] = 255;
	game->colors[1][2] = 0;
	game->colors[2][0] = 0;
	game->colors[2][1] = 0;
	game->colors[2][2] = 255;
	game->colors[3][0] = 255;
	game->colors[3][1] = 255;
	game->colors[3][2] = 0;
	game->colors[4][0] = 255;
	game->colors[4][1] = 0;
	game->colors[4][2] = 255;
	game->colors[5][0] = 0;
	game->colors[5][1] = 255;
	game->colors[5][2] = 255;
	game->mode = MODE_CLASSIC;
	game->canPlay = 0;
	game->receivedGrid = 0;
}

void game_start(s_Game *game) {
	game->timeStarted = 0;
	game->timeFinished = 0;

	if (game_is(game, MODE_TIMED)) {
		game->timeStarted = SDL_GetTicks();
	}

	char isMultiplayer = game_is(game, MODE_MULTIPLAYER);
	game->canPlay = 0;
	if (!isMultiplayer || (isMultiplayer && game->socketConnection.type == SERVER)) {
		_generateGrid(game);

		if (isMultiplayer) {
			//send grid to players
			game_broadcastGrid(game);
		}

		_generateFirstPlayer(game);

		if (game->currentPlayerIndex == 0) {
			game->canPlay = 1;
		}
		else {
			// notify first player
			game_notifyCurrentPlayerTurn(
				game,
				MULTIPLAYER_MESSAGE_TYPE_PLAYER_TURN
			);
		}
	}

	// program main loop
	game->iSelectedColor = 0;
	game->iTurns = 1;
}

void game_restart(s_Game *game) {
	game_start(game);
}

void game_finish(s_Game *game, const char won) {
	if (game_is(game, MODE_TIMED)) {
		game->timeFinished = SDL_GetTicks();
		if (won) {
			high_score_save(game->timeFinished - game->timeStarted, game->iTurns);
		}
	}
}

void game_clean(s_Game *game) {
	TTF_CloseFont(game->scoreFont);
	game->scoreFont = NULL;
	TTF_CloseFont(game->endFont);
	game->endFont = NULL;
	TTF_CloseFont(game->menuFont);
	game->menuFont = NULL;
	TTF_CloseFont(game->selectedMenuFont);
	game->selectedMenuFont = NULL;
	TTF_CloseFont(game->highScoreFont);
	game->highScoreFont = NULL;
	TTF_CloseFont(game->highScoreTitleFont);
	game->highScoreTitleFont = NULL;

	if (game_is(game, MODE_MULTIPLAYER)) {
		multiplayer_clean(&game->socketConnection);
	}
}

game_play_result game_play(s_Game *game, int selectedColor) {
	char isMultiplayer = game_is(game, MODE_MULTIPLAYER);
	if (isMultiplayer && game->socketConnection.type == CLIENT) {
		_notifyServerPlayerTurn(game);
		return CLIENT_PLAYED;
	}
	else if (game_selectColor(game, selectedColor) <= 0) {
		return INVALID_PLAY;
	}

	char boardFull = game_checkBoard(game);
	char allTurnsDone = (game->iTurns == MAX_TURNS);
	game_play_result result;
	if (boardFull || allTurnsDone) {
		game_finish(game, !allTurnsDone);
		result = boardFull ? GAME_WON : GAME_LOST;
	}
	else {
		if (!isMultiplayer) {
			game->iTurns++;
		}
		else {
			game_notifyCurrentPlayerTurn(game, 0);
			game_selectNextPlayer(game);
			game_notifyCurrentPlayerTurn(game, 1);
			game_broadcastGrid(game);
		}

		result = END_TURN;
	}

	return result;
}


char game_is(s_Game *game, game_mode mode) {
	return game->mode == mode;
}

void game_setMode(s_Game *game, game_mode mode) {
	game->mode = mode;
}

void game_getTimer(s_Game *game, char *timer) {
	Uint32 seconds = 0,
		minutes = 0,
		totalSeconds,
		endTime;

	if (game->timeFinished == 0) {
		endTime = SDL_GetTicks();
	}
	else {
		endTime = game->timeFinished;
	}

	totalSeconds = (endTime - game->timeStarted) / 1000;
	seconds = totalSeconds % 60;
	minutes = totalSeconds / 60;
	snprintf(timer, 6, "%02d:%02d", minutes, seconds);
}


char game_checkBoard(s_Game* game) {
	signed char color = -1;
	int i, j;
	for (j = 0; j < HEIGHT_GRID; ++j) {
		for (i = 0; i < WIDTH_GRID; ++i) {
			if (color != -1 && game->grid[j][i] != color) {
				return 0;
			}
			else {
				color = game->grid[j][i];
			}
		}
	}

	return 1;
}

char game_selectColor(s_Game* game, int color) {
	int startX, startY;
	startX = g_startPositionPlayers[game->currentPlayerIndex][0];
	startY = g_startPositionPlayers[game->currentPlayerIndex][1];
	char ret = _spreadColor(game, color, startX, startY);

	return ret;
}

void game_getNeighbours(int x, int y, int neighbours[4][2], int* nbNeighbours) {
	(*nbNeighbours) = 0;
	if (x > 0) {
		neighbours[*nbNeighbours][0] = x - 1;
		neighbours[*nbNeighbours][1] = y;
		(*nbNeighbours) += 1;
	}

	if (x < WIDTH_GRID - 1) {
		neighbours[*nbNeighbours][0] = x + 1;
		neighbours[*nbNeighbours][1] = y;
		(*nbNeighbours) += 1;
	}

	if (y > 0) {
		neighbours[*nbNeighbours][0] = x;
		neighbours[*nbNeighbours][1] = y - 1;
		(*nbNeighbours) += 1;
	}

	if (y < HEIGHT_GRID - 1) {
		neighbours[*nbNeighbours][0] = x;
		neighbours[*nbNeighbours][1] = y + 1;
		(*nbNeighbours) += 1;
	}
}

void game_setGrid(s_Game* game, s_TCPpacket packet) {
	int i, j;
	for (j = 0; j < HEIGHT_GRID; ++j) {
		for (i = 0; i < WIDTH_GRID; ++i) {
			game->grid[j][i] = packet.data[j * WIDTH_GRID + i];
		}
	}

	game->receivedGrid = 1;
}


void game_broadcastGrid(s_Game *game) {
	s_TCPpacket packet;
	packet.type = MULTIPLAYER_MESSAGE_TYPE_GRID;
	packet.size = 196;
	int nbSockets;
	for (nbSockets = 0; nbSockets < game->socketConnection.nbConnectedSockets; ++nbSockets) {
		_setRotatedGridPacket(game, &packet, nbSockets);
		multiplayer_send_message(game->socketConnection, nbSockets, packet);
	}
}

void _setRotatedGridPacket(s_Game *game, s_TCPpacket *packet, int playerIndex) {
	if (playerIndex == 0) {
		for (int j = HEIGHT_GRID - 1; j >= 0; --j) {
			for (int i = WIDTH_GRID - 1; i >= 0; --i) {
				int x = WIDTH_GRID - 1 - i;
				int y = HEIGHT_GRID - 1 - j;
				packet->data[y * WIDTH_GRID + x] = game->grid[j][i];
			}
		}
	}
	else if (playerIndex == 1) {
		for (int i = WIDTH_GRID - 1; i >= 0; --i) {
			for (int j = 0; j < HEIGHT_GRID; ++j) {
				int x = WIDTH_GRID - 1 - i;
				packet->data[x * HEIGHT_GRID + j] = game->grid[j][i];
			}
		}
	}
	else {
		for (int i = 0; i < WIDTH_GRID; ++i) {
			for (int j = HEIGHT_GRID - 1; j >= 0; --j) {
				int y = HEIGHT_GRID - 1 - j;
				packet->data[i * HEIGHT_GRID + y] = game->grid[j][i];
			}
		}
	}
}

void game_notifyCurrentPlayerTurn(s_Game *game, char isTurn) {
	if (game->currentPlayerIndex == 0) {
		game->canPlay = isTurn;
		return;
	}

	s_TCPpacket packet;
	if (isTurn) {
		packet.type = MULTIPLAYER_MESSAGE_TYPE_PLAYER_TURN;
	}
	else {
		packet.type = MULTIPLAYER_MESSAGE_TYPE_PLAYER_END_TURN;
	}
	packet.size = 0;
	multiplayer_send_message(
		game->socketConnection,
		game->currentPlayerIndex - 1,
		packet
	);
}

void game_selectNextPlayer(s_Game *game) {
	if (game_is(game, MODE_MULTIPLAYER)) {
		game->currentPlayerIndex = (game->currentPlayerIndex + 1) % (game->socketConnection.nbConnectedSockets + 1);
	}
}

/**
 * Will return 0 if the game is a client and if the server disconnected, 1
 * otherwise
 */
char game_processIncomingPackets(s_Game *game) {
	if (game->socketConnection.type == SERVER) {
		_processServerPackets(game);
	}
	else if (!_processClientPackets(game)) {
		return 0;
	}

	return 1;
}


/** PRIVATE FUNCTIONS **/

void _processServerPackets(s_Game *game) {
	s_TCPpacket packet;
	int indexSocketSendingMessage = -1;
	char foundMessage = multiplayer_check_clients(
		&game->socketConnection,
		&packet,
		&indexSocketSendingMessage
	);

	// the current player played and we received its choice
	if (foundMessage == MESSAGE_RECEIVED && packet.type == MULTIPLAYER_MESSAGE_TYPE_PLAYER_TURN) {
		// check message comes from good socket
		if (indexSocketSendingMessage != game->currentPlayerIndex) {
			// comes from someone else, ignore it
			return;
		}

		game_play(game, packet.data[0]);
	}
}

char _processClientPackets(s_Game *game) {
	s_TCPpacket packet;
	char state = multiplayer_check_server(&game->socketConnection, &packet);
	if (state == CONNECTION_LOST) {
		return 0;
	}
	else if (state == MESSAGE_RECEIVED) {
		if (packet.type == MULTIPLAYER_MESSAGE_TYPE_GRID) {
			game_setGrid(game, packet);
			game->receivedGrid = 1;
		}
		// We received a message from the server telling us it is our turn
		// to play
		else if (packet.type == MULTIPLAYER_MESSAGE_TYPE_PLAYER_TURN) {
			game->canPlay = 1;
		}
		// The server is now telling us it is not our turn anymore
		else if (packet.type == MULTIPLAYER_MESSAGE_TYPE_PLAYER_END_TURN) {
			game->canPlay = 0;
		}
	}

	return 1;
}

void _generateFirstPlayer(s_Game *game) {
	if (!game_is(game, MODE_MULTIPLAYER)) {
		game->currentPlayerIndex = 0;
	}
	else {
		time_t t;
		srand((unsigned) time(&t));
		game->currentPlayerIndex = rand() % (game->socketConnection.nbConnectedSockets + 1);
	}
}

/**
 * Generate a random grid
 */
void _generateGrid(s_Game* game) {
	int i, j;
	time_t t;

	srand((unsigned) time(&t));
	for (j = 0; j < HEIGHT_GRID; ++j) {
		for (i = 0; i < WIDTH_GRID; ++i) {
			game->grid[j][i] = rand() % NB_COLORS;
		}
	}

	game->receivedGrid = 1;
}

/**
 * Change the colors of the grid from [startX, startY] with selectedColor
 */
char _spreadColor(s_Game *game, int selectedColor, int startX, int startY) {
	char toVisitFlag = 0x1,
		 visitedFlag = 0x2;
	int i, j, nbToVisit, oldColor;
	int *toVisit;
	int **visited;

	oldColor = game->grid[startY][startX];
	if (selectedColor == oldColor) {
		return 0;
	}

	visited = (int **) malloc(HEIGHT_GRID * sizeof(int *));
	for (i = 0; i < HEIGHT_GRID; i++) {
		visited[i] = (int *) malloc(WIDTH_GRID * sizeof(int));
	}

	toVisit = (int *) malloc(WIDTH_GRID * HEIGHT_GRID * sizeof(int *));

	for (j = 0; j < HEIGHT_GRID; ++j) {
		for (i = 0; i < WIDTH_GRID; ++i) {
			visited[j][i] = 0;
			toVisit[j * WIDTH_GRID + i] = 0;
		}
	}

	toVisit[0] = startY * WIDTH_GRID + startX;
	visited[startY][startX] |= toVisitFlag;
	nbToVisit = 1;

	while (nbToVisit > 0) {
		int x, y, next = utils_popArray(toVisit, &nbToVisit);

		x = next % WIDTH_GRID;
		y = next / WIDTH_GRID;
		visited[y][x] |= visitedFlag;
		game->grid[y][x] = selectedColor;

		int neighbours[4][2];
		int nbNeighbours;
		game_getNeighbours(x, y, neighbours, &nbNeighbours);
		for (i = 0; i < nbNeighbours; ++i) {
			if (
				visited[neighbours[i][1]][neighbours[i][0]] == 0
				&& game->grid[neighbours[i][1]][neighbours[i][0]] == oldColor
			) {
				toVisit[nbToVisit++] = neighbours[i][1] * WIDTH_GRID + neighbours[i][0];
				visited[neighbours[i][1]][neighbours[i][0]] = toVisitFlag;
			}
		}
	}

	for (i = 0; i < HEIGHT_GRID; i++) {
		free(visited[i]);
	}
	free(visited);
	free(toVisit);

	return 1;
}

void _notifyServerPlayerTurn(s_Game *game) {
	s_TCPpacket packet;
	packet.type = MULTIPLAYER_MESSAGE_TYPE_PLAYER_TURN;
	packet.size = 1;
	packet.data[0] = game->iSelectedColor;
	multiplayer_send_message(game->socketConnection, -1, packet);
}
