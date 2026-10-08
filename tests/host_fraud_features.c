// Teste no PC: compila o MESMO fraud_features.c do firmware e imprime as features do replay.
#include <stdio.h>
#include "fraud_features.h"
#include "replay_data.h"
int main(void){
  static card_state_t cards[REPLAY_N_CARDS]; uint32_t clk[REPLAY_N_CARDS];
  for(int i=0;i<REPLAY_N_CARDS;i++){card_init(&cards[i],kReplayCardAge[i]); clk[i]=1593561600u;}
  for(int i=0;i<REPLAY_LEN;i++){const replay_tx_t*t=&kReplay[i]; clk[t->card]+=t->dt_s; fraud_feat_t f;
    fraud_features(&cards[t->card],clk[t->card],t->amount,t->category,t->hour,t->minute,&f);
    printf("%d",i); for(int k=0;k<FRAUD_N_FEATURES;k++) printf(",%.6f",f.x[k]); printf("\n");}
}
