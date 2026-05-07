
typedef struct {
	SINT32	lastclk2;
	SINT32	counter;
} _ARTIC, *ARTIC;


#ifdef __cplusplus
extern "C" {
#endif

void artic_callback(void);

#if defined(VITA_NP2_PORT)
extern UINT32 vita_artic_out5f_count;
extern UINT32 vita_artic_in5c_count;
extern UINT32 vita_artic_in5d_count;
extern UINT32 vita_artic_in5f_count;
extern UINT32 vita_artic_forceexit_count;
UINT32 vita_artic_getcnt_diag(void);
#endif

void artic_reset(void);
void artic_bind(void);
REG16 IOINPCALL artic_r16(UINT port);

#ifdef __cplusplus
}
#endif

