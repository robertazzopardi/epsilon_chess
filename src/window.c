#include "window.h"
#include "board.h"
#include "piece.h"
#include <SDL.h>
#include <SDL_image.h>

#define SQUARE_COUNT 64
#define TITLE "Chess"
#define FRAME_DELAY 1000 / 60

Window mainWindow;

void handleEvents(SDL_Event *event, bool *running, MouseEvent *mEvent) {
    while (SDL_PollEvent(event)) {
        switch (event->type) {
        case SDL_QUIT:
            *running = !running;
            break;
        case SDL_MOUSEBUTTONUP:
            if (mEvent->LMBDown && event->button.button == SDL_BUTTON_LEFT) {
                if (mEvent->piece->rect != NULL) {
                    if (canMove(mEvent)) {
                        checkIfFirstMove(mEvent);
                        board.moveCount++;

                        alignPiece(mEvent);
                    } else {
                        mEvent->piece->rect->x = mEvent->oldPos.x;
                        mEvent->piece->rect->y = mEvent->oldPos.y;
                    }
                }
                mEvent->LMBDown = false;
                mEvent->piece->rect = NULL;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (!mEvent->LMBDown && event->button.button == SDL_BUTTON_LEFT) {
                mEvent->LMBDown = true;
                checkIfPiece(mEvent, &board.p1);
                checkIfPiece(mEvent, &board.p2);

                // save old position
                if (mEvent->piece->rect) {
                    mEvent->oldPos.x = mEvent->piece->rect->x;
                    mEvent->oldPos.y = mEvent->piece->rect->y;
                }

                toggleBoardSquare(&mEvent->mousePos, &mEvent->square);
            }
            break;
        case SDL_MOUSEMOTION: {
            mEvent->mousePos.x = event->motion.x;
            mEvent->mousePos.y = event->motion.y;

            if (mEvent->LMBDown && mEvent->piece->rect != NULL) {
                mEvent->piece->rect->x = mEvent->mousePos.x - mEvent->offset.x;
                mEvent->piece->rect->y = mEvent->mousePos.y - mEvent->offset.y;
            }
        } break;
        }
    }
}

void gameLoop() {
    // controls annimation loop
    bool running = true;

    SDL_Event event = {0};

    MouseEvent mEvent = {0};
    mEvent.piece = calloc(1, sizeof(*mEvent.piece));

    // annimation loop
    while (running) {
        handleEvents(&event, &running, &mEvent);

        SDL_SetRenderDrawColor(mainWindow.rend, 0, 0, 0, 255);

        // clears the screen
        SDL_RenderClear(mainWindow.rend);

        drawBoard();

        drawPieces(mainWindow.rend);

        // triggers the double buffers for multiple rendering
        SDL_RenderPresent(mainWindow.rend);

        // calculates to 60 fps
        SDL_Delay(FRAME_DELAY);
    }

    free(mEvent.piece);
}

void cleanUpWindow() {
    // destroy renderer
    SDL_DestroyRenderer(mainWindow.rend);

    // destroy window
    SDL_DestroyWindow(mainWindow.win);

    mainWindow.win = NULL;
    mainWindow.rend = NULL;

    IMG_Quit();
    // close SDL
    SDL_Quit();
}

void initialise() {
    // retutns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        exit(0);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");

    mainWindow.win = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);

    mainWindow.rend = SDL_CreateRenderer(mainWindow.win, -1, SDL_RENDERER_ACCELERATED);

    makeBoard();

    makePieces(mainWindow.rend);

    gameLoop();

    cleanUpPieces();
    cleanUpWindow();
    cleanUpBoard();
}
