//#include <filesystem>
#pragma once
#include <thread>
#include <memory>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <fstream>
#include <cassert>
#include "search.h"
#include "evaluate.h"
#include "search.h"

const int PRECISION = 5;
const int NPOS = 2500000;
const int TERMS = 964;
const int BUCKET_SIZE = 1LL * NPOS * TERMS / 64;

struct Position {
  std::string fen;
  double result;
};

TunePos position[NPOS + 5];

int nrPos;
Position texelPos[NPOS + 5];
EvalTraceEntry *stack;
int stackSize = BUCKET_SIZE;

double sigmoid(double k, int s) {
  return 1.0 / (1.0 + pow(10.0, -k * s / 400.0));
}

std::mutex M; /// for debugging data races
int weights[TERMS + 5];
EvalTrace empty;

/// TEXEL tuning method

bool openFile(std::ifstream &stream, std::string path) {
  if(stream.is_open())
    stream.close();
  stream.open(path, std::ifstream::in);
  return stream.is_open();
}

void load(std::ifstream &stream) {

  std::string line;
  Info info[1];
  Board board;

  info->timeset = 0;
  info->startTime = 0;


  while(getline(stream, line)) {
    Position pos;

    if(nrPos % 100000 == 0)
      std::cout << nrPos << "\n";

    auto fenEnd = line.find_last_of(' ', std::string::npos) + 1;

    std::string result = "";
    int i = fenEnd + 1;
    while(i < (int)line.size() && line[i] != '"')
      result += line[i++];

    if(result == "1/2-1/2") /// for zurichess positions
      pos.result = 0.5;
    else if(result == "1-0")
      pos.result = 1.0;
    else if(result == "0-1")
      pos.result = 0.0;
    else if(result == "Draw") /// for lichess-quiet positions
      pos.result = 0.5;
    else if(result == "White")
      pos.result = 1.0;
    else if(result == "Black")
      pos.result = 0.0;

    //cout << pos.result << "\n";

    fenEnd -= 2;

    while(line[fenEnd] != ' ')
      fenEnd--;

    std::string fenString = line.substr(0, fenEnd);
    fenString += " 0 0";


    pos.fen = fenString;

    /*searcher._setFen(pos.fen);
    searcher.setTime(info);

    searcher.quiesce(-INF, INF);

    for(int i = 0; i < searcher.pvTableLen[0]; i++)
      searcher._makeMove(searcher.pvTable[0][i]);

    pos.fen = searcher.board.fen();*/


    //Board &board[1];

    //board.setFen(pos.fen);
    //board.print();

    //std::cout << pos.fen << "\n";

    board.setFen(pos.fen);

    trace = empty;
    //std::cout << trace.phase << "\n";
    int initScore = evaluate(board);

    getTraceEntries(trace);

    int num = trace.nrEntries;

    if(num > stackSize) {
      stackSize = BUCKET_SIZE;
      stack = (EvalTraceEntry*)calloc(BUCKET_SIZE, sizeof(EvalTraceEntry));
    }

    position[nrPos].entries = stack;
    stack += num;
    stackSize -= num;

    position[nrPos].nrEntries = num;
    for(int i = 0; i < num; i++)
      position[nrPos].entries[i] = trace.entries[i];

    position[nrPos].phase = trace.phase;
    position[nrPos].turn = board.turn;
    for(int i = 0; i < 2; i++) {
      position[nrPos].kingDanger[i] = trace.kingDanger[i];
      position[nrPos].others[i] = trace.others[i];
    }

    int traceScore = evaluateTrace(position[nrPos], weights);

    if(initScore != traceScore) {
      std::cout << initScore << " " << traceScore << "\n";
      board.print();
    }

    /*if(nrPos == 0) {
      std::cout << pos.fen << "\n";
      std::cout << position[nrPos].nrEntries << "\n";
      for(int i = 0; i < position[nrPos].nrEntries; i++)
        std::cout << position[nrPos].entries[i].ind << " ";
      std::cout << "\n";
    }*/

    texelPos[nrPos++] = pos;
  }

  /*for(int i = 0; i < 10; i++) {
    std::cout << position[i].nrEntries << "\n";
    for(int j = 0; j < position[i].nrEntries; j++)
      std::cout << position[i].entries[j].ind << " ";
    std::cout << "\n";
  }

  std::cout << total * sizeof(EvalTraceEntry) << "\n";*/
}

void loadWeights() {
  int ind = 0;
  for(int i = MG; i <= EG; i++)
    weights[ind++] = (doubledPawnsPenalty[i]);
  for(int i = MG; i <= EG; i++)
    weights[ind++] = (isolatedPenalty[i]);
  for(int i = MG; i <= EG; i++)
    weights[ind++] = (backwardPenalty[i]);
  for(int s = MG; s <= EG; s++) {
    for(int i = PAWN; i <= QUEEN; i++)
      weights[ind++] = (mat[s][i]);
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 1; i < 7; i++)
      weights[ind++] = (passedBonus[s][i]);
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 1; i < 7; i++)
      weights[ind++] = (connectedBonus[s][i]);
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 1; i < 4; i++)
      weights[ind++] = (pawnShield[s][i]);
  }

  /// skipping king attack parameters

  for(int s = MG; s <= EG; s++) {
    for(int i = KNIGHT; i <= BISHOP; i++)
      weights[ind++] = (outpostBonus[s][i]);
  }

  for(int s = MG; s <= EG; s++) {
    for(int i = KNIGHT; i <= BISHOP; i++)
      weights[ind++] = (outpostHoleBonus[s][i]);
  }
  for(int s = MG; s <= EG; s++)
    weights[ind++] = (rookOpenFile[s]);
  for(int s = MG; s <= EG; s++)
    weights[ind++] = (rookSemiOpenFile[s]);
  for(int s = MG; s <= EG; s++)
    weights[ind++] = (bishopPair[s]);
  for(int s = MG; s <= EG; s++)
    weights[ind++] = (longDiagonalBishop[s]);

  for(int s = MG; s <= EG; s++) {
    weights[ind++] = (trappedRook[s]);
  }

  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 9; i++)
      weights[ind++] = (mobilityBonus[KNIGHT][s][i]);
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 14; i++)
      weights[ind++] = (mobilityBonus[BISHOP][s][i]);
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 15; i++)
      weights[ind++] = (mobilityBonus[ROOK][s][i]);
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 28; i++)
      weights[ind++] = (mobilityBonus[QUEEN][s][i]);
  }

  for(int i = PAWN; i <= KING; i++) {
    for(int s = MG; s <= EG; s++) {
      for(int j = A1; j <= H8; j++)
        weights[ind++] = (bonusTable[i][s][j]);
    }
  }
}

void saveWeights() {
  int ind = 0;
  for(int i = MG; i <= EG; i++)
    doubledPawnsPenalty[i] = weights[ind++];
  for(int i = MG; i <= EG; i++)
    isolatedPenalty[i] = weights[ind++];
  for(int i = MG; i <= EG; i++)
    backwardPenalty[i] = weights[ind++];
  for(int s = MG; s <= EG; s++) {
    for(int i = PAWN; i <= QUEEN; i++)
      mat[s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 1; i < 7; i++)
      passedBonus[s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 1; i < 7; i++)
      connectedBonus[s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 1; i < 4; i++)
      pawnShield[s][i] = weights[ind++];
  }


  for(int s = MG; s <= EG; s++) {
    for(int i = KNIGHT; i <= BISHOP; i++)
      outpostBonus[s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = KNIGHT; i <= BISHOP; i++)
      outpostHoleBonus[s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++)
    rookOpenFile[s] = weights[ind++];
  for(int s = MG; s <= EG; s++)
    rookSemiOpenFile[s] = weights[ind++];
  for(int s = MG; s <= EG; s++)
    bishopPair[s] = weights[ind++];
  for(int s = MG; s <= EG; s++)
    longDiagonalBishop[s] = weights[ind++];

  for(int s = MG; s <= EG; s++)
    trappedRook[s] = weights[ind++];
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 9; i++)
      mobilityBonus[KNIGHT][s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 14; i++)
      mobilityBonus[BISHOP][s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 15; i++)
      mobilityBonus[ROOK][s][i] = weights[ind++];
  }
  for(int s = MG; s <= EG; s++) {
    for(int i = 0; i < 28; i++)
      mobilityBonus[QUEEN][s][i] = weights[ind++];
  }

  //cout << ind << "\n";

  for(int i = PAWN; i <= KING; i++) {
    for(int s = MG; s <= EG; s++) {
      for(int j = A1; j <= H8; j++)
        bonusTable[i][s][j] = weights[ind++];
    }
  }
}

void printWeights() {
  int ind = 0;

  std::cout << "int doubledPawnsPenalty[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int isolatedPenalty[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int backwardPenalty[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int mat[2][7] = {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "    {0, ";
    for(int i = PAWN; i <= QUEEN; i++)
      std::cout << weights[ind++] << ", ";
    std::cout << "0},\n";
  }
  std::cout << "};\n";
  std::cout << "const int phaseVal[] = {0, 0, 1, 1, 2, 4};\n";
  std::cout << "const int maxWeight = 16 * phaseVal[PAWN] + 4 * phaseVal[KNIGHT] + 4 * phaseVal[BISHOP] + 4 * phaseVal[ROOK] + 2 * phaseVal[QUEEN];\n";

  std::cout << "int passedBonus[2][7] = {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "  {0";
    for(int i = 1; i < 7; i++)
      std::cout << ", " << weights[ind++];
    std::cout << "},\n";
  }
  std::cout << "};\n";

  std::cout << "int connectedBonus[2][7] = {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "  {0";
    for(int i = 1; i < 7; i++)
      std::cout << ", " << weights[ind++];
    std::cout << "},\n";
  }
  std::cout << "};\n";

  std::cout << "int kingAttackWeight[] = {0, 0";
  for(int i = KNIGHT; i <= QUEEN; i++)
    std::cout << ", " << kingAttackWeight[i];
  std::cout << "};\n";
  std::cout << "int SafetyTable[100] = {\n";
  for(int i = 0; i < 10; i++) {
    std::cout << "  ";
    for(int j = 0; j < 10; j++) {
      std::cout << SafetyTable[i * 10 + j] << ", ";
    }
    std::cout << "\n";
  }
  std::cout << "};\n";

  std::cout << "int pawnShield[2][4] = {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "  {0";
    for(int i = 1; i < 4; i++)
      std::cout << ", " << weights[ind++];
    std::cout << "},\n";
  }
  std::cout << "};\n";

  std::cout << "int outpostBonus[2][4] = {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "  {0, 0";
    for(int i = KNIGHT; i <= BISHOP; i++)
      std::cout << ", " << weights[ind++];
    std::cout << "},\n";
  }
  std::cout << "};\n";

  std::cout << "int outpostHoleBonus[2][4] = {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "  {0, 0";
    for(int i = KNIGHT; i <= BISHOP; i++)
      std::cout << ", " << weights[ind++];
    std::cout << "},\n";
  }
  std::cout << "};\n";

  std::cout << "int rookOpenFile[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int rookSemiOpenFile[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int bishopPair[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int longDiagonalBishop[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int trappedRook[2] = {";
  for(int i = MG; i <= EG; i++)
    std::cout << weights[ind++] << ", ";
  std::cout << "};\n";

  std::cout << "int mobilityBonus[7][2][30] = {\n";
  std::cout << "    {},\n";
  std::cout << "    {},\n";
  std::cout << "    {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "        {";
    for(int i = 0; i < 9; i++)
      std::cout << weights[ind++] << ", ";
    std::cout << "},\n";
  }
  std::cout << "    },\n";
  std::cout << "    {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "        {";
    for(int i = 0; i < 14; i++)
      std::cout << weights[ind++] << ", ";
    std::cout << "},\n";
  }
  std::cout << "    },\n";
  std::cout << "    {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "        {";
    for(int i = 0; i < 15; i++)
      std::cout << weights[ind++] << ", ";
    std::cout << "},\n";
  }
  std::cout << "    },\n";
  std::cout << "    {\n";
  for(int s = MG; s <= EG; s++) {
    std::cout << "        {";
    for(int i = 0; i < 28; i++)
      std::cout << weights[ind++] << ", ";
    std::cout << "},\n";
  }
  std::cout << "    }\n";
  std::cout << "};\n";

  std::cout << "int bonusTable[7][2][64] = {\n";
  std::cout << "    {},\n";
  for(int i = PAWN; i <= KING; i++) {
    std::cout << "    {\n";
    for(int s = MG; s <= EG; s++) {
      std::cout << "        {\n            ";
      for(int j = A1; j <= H8; j++) {
        std::cout << weights[ind++] << ", ";
        if(j % 8 == 7)
          std::cout << "\n            ";
      }
      std::cout << "\n        },\n";
    }
    std::cout << "    },\n";
  }
  std::cout << "};\n";
}

void rangeEvalError(std::atomic <double> & error, double k, int l, int r) {

  double errorRange = 0;
  //Board board;

  for(int i = l; i < r; i++) {
    //board.setFen(texelPos[i].fen);
    //board.print();
    int eval = evaluateTrace(position[i], weights);
    //int eval = evaluate(board);
    /*
    if(evalInit != evaltr) {
      M.lock();
      std::cout << i << "\n";
      board.print();
      for(int j = 0; j < position[i].nrEntries; j++)
        std::cout << position[i].entries[j].ind << " " << position[i].entries[j].val << "\n";
      std::cout << evalInit << " " << evaltr << "\n";
      M.unlock();
      assert(0);
    }*/
    int score = eval * (position[i].turn == WHITE ? 1 : -1);

    errorRange += pow(texelPos[i].result - sigmoid(k, score), 2);
    //cout << errorRange << "\n";
  }
  M.lock();

  error = error + errorRange;

  M.unlock();
}

double evalError(double k, int nrThreads) {
  std::atomic <double> error{};
  double share = 1.0 * nrPos / nrThreads;
  double ind = 0;

  std::vector <std::thread> threads(nrThreads);

  for(auto &t : threads) {
    t = std::thread{ rangeEvalError, ref(error), k, int(floor(ind)), int(floor(ind + share) - 1) };
    ind += share;
  }

  for(auto &t : threads)
    t.join();

  assert(floor(ind) == nrPos);

  return error / nrPos;
}

double bestK(int nrThreads, long double ST) {
  double k = 0;
  double mn = std::numeric_limits <double> :: max();

  std::cout.precision(PRECISION + 2);

  for(int i = 0; i <= PRECISION; i++) {
    std::cout << "iteration " << i + 1 << " ...\n";

    double unit = pow(10.0, -i), range = 10.0 * unit, r = k + range;

    for(double curr = std::max(k - range, 0.0); curr <= r; curr += unit) {
      double error = evalError(curr, nrThreads);

      std::cout << "current K = " << curr << ", current error = " << error << ", time = " << (getTime() - ST) / 1000.0 << "s\n";

      if(error < mn) {
        mn = error;
        k = curr;
      }
    }

    std::cout << "K = " << k << ", error = " << mn << std::endl;
  }

  return k;
}

bool isBetter(double &mn, double k, int nrThreads) {
  saveWeights();

  double error = evalError(k, nrThreads);

  if(error < mn) {
    if(mn - error > mn / nrThreads / 2) {
      return 0;
    }
    mn = error;
    return 1;
  }

  return 0;
}

void tune(int nrThreads, std::string path) {
  //int nrThreads = 8;

  stack = (EvalTraceEntry*)calloc(BUCKET_SIZE, sizeof(EvalTraceEntry));

  std::ifstream stream;
  if(openFile(stream, path))
    std::cout << "opening file.." << std::endl;

  loadWeights();

  saveWeights();

  printWeights();

  load(stream);

  double start = getTime();

  double k = bestK(nrThreads, start);

  double startTime = getTime();

  double errorStart = evalError(k, nrThreads), errorMin = errorStart;
  double otherTime = getTime();
  int i = 1;

  std::cout << "best K = " << k << "\n";

  std::cout << "start evaluation error: " << errorStart << std::endl;
  std::cout << "expected iteration time: " << (uint64_t)(otherTime - startTime) * TERMS * 2 / 1000 << "s" << std::endl;

  //cout << evaluate(texelPos[0].board) << endl;

  while(true) {
    std::cout << "epoch " << i << " starting..." << std::endl;

    double ST = getTime();
    double lst = errorMin;
    int ind = 0;

    for(int i = 0; i < TERMS; i++) {
      int temp = weights[i];
      bool improve = 0;

      //cout << "parameter " << ind << "..\n";

      weights[i] += 1;

      if(isBetter(errorMin, k, nrThreads))
        improve = 1;
      else {
        weights[i] -= 2;
        improve = isBetter(errorMin, k, nrThreads);
      }

      if(!improve)
        weights[i] = temp;

      ind++;
    }

    double ET = getTime();

    std::cout << "time taken for iteration: " << (uint64_t)(ET - ST) / 1000 << " s\n";
    i++;
    printWeights();
    saveWeights();

    std::cout << "evaluation error: " << errorMin << "\n";

    if(errorMin == lst)
      break;
  }

  //cout << passedBonus[6] << "\n";

  //errorMin = evalError(texelPos, k);

  std::cout << "evaluation error: " << errorStart << " -> " << errorMin << std::endl;

  std::cout << "Time taken: " << (clock() - startTime) / 1000.0 << " s" << std::endl;
}
