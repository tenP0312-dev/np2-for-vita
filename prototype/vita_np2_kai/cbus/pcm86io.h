

#ifdef __cplusplus
extern "C" {
#endif

#if defined(VITA_NP2_PORT)
extern UINT32 vita_pcm86_in466_count;
extern UINT32 vita_pcm86_in468_count;
extern UINT32 vita_pcm86_out46c_count;
extern REG8 vita_pcm86_last466;
extern REG8 vita_pcm86_last468;
extern REG8 vita_pcm86_last46c;
extern UINT32 vita_pcm86_kai_intrq_count;
extern UINT32 vita_pcm86_kai_cb_irq_count;
extern UINT32 vita_pcm86_kai_force_irq_count;
#endif
void pcm86io_bind(void);

#ifdef __cplusplus
}
#endif

