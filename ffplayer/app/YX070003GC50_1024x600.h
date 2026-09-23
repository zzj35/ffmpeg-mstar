/* ================= YX-070003GC50 1024x600 ================= */
MI_PANEL_ParamConfig_t stPanelParam_YX070003GC50 =
{
    "YX070003GC50_1024x600", // const char *m_pPanelName;
    0, // MS_U8 m_bPanelDither :1;
    E_MI_PNL_LINK_TTL, // MHAL_DISP_ApiPnlLinkType_e m_ePanelLinkType :4;

    // Board related setting
    0,  // MS_U8 m_bPanelDualPort  :1;
    0,  // MS_U8 m_bPanelSwapPort  :1;
    0,  // MS_U8 m_bPanelSwapOdd_ML    :1;
    0,  // MS_U8 m_bPanelSwapEven_ML   :1;
    1,  // MS_U8 m_bPanelSwapOdd_RB    :1;
    1,  // MS_U8 m_bPanelSwapEven_RB   :1;
    0,  // MS_U8 m_bPanelSwapLVDS_POL  :1;
    0,  // MS_U8 m_bPanelSwapLVDS_CH   :1;
    0,  // MS_U8 m_bPanelPDP10BIT      :1;
    0,  // MS_U8 m_bPanelLVDS_TI_MODE  :1;

    // For TTL Only
    0,  // MS_U8 m_ucPanelDCLKDelay;
    0,  // MS_U8 m_bPanelInvDCLK   :1;
    0,  // MS_U8 m_bPanelInvDE     :1;
    1,  // MS_U8 m_bPanelInvHSync  :1;   // HSD 负极性，反转
    1,  // MS_U8 m_bPanelInvVSync  :1;   // VSD 负极性，反转

    // Output driving current
    3,  // MS_U8 m_ucPanelDCKLCurrent;
    3,  // MS_U8 m_ucPanelDECurrent;
    3,  // MS_U8 m_ucPanelODDDataCurrent;
    3,  // MS_U8 m_ucPanelEvenDataCurrent;

    // panel on/off timing
    30,  // MS_U16 m_wPanelOnTiming1;
    400, // MS_U16 m_wPanelOnTiming2;
    80,  // MS_U16 m_wPanelOffTiming1;
    30,  // MS_U16 m_wPanelOffTiming2;

    // panel timing spec. (HV/SYNC mode)
    20,  // MS_U8 m_ucPanelHSyncWidth;      // thpw = 20
    140, // MS_U8 m_ucPanelHSyncBackPorch;  // thbp = 140
    3,   // MS_U8 m_ucPanelVSyncWidth;      // tvpw = 3
    20,  // MS_U8 m_ucPanelVBackPorch;      // tvbp = 20

    160, // MS_U16 m_wPanelHStart;          // 20 + 140
    23,  // MS_U16 m_wPanelVStart;          // 3 + 20
    1024,// MS_U16 m_wPanelWidth;
    600, // MS_U16 m_wPanelHeight;

    1364,// MS_U16 m_wPanelMaxHTotal;
    1344,// MS_U16 m_wPanelHTotal;          // th = 1344
    1324,// MS_U16 m_wPanelMinHTotal;
    645, // MS_U16 m_wPanelMaxVTotal;
    635, // MS_U16 m_wPanelVTotal;          // tv = 635
    625, // MS_U16 m_wPanelMinVTotal;
    57,  // MS_U8 m_dwPanelMaxDCLK;
    51,  // MS_U8 m_dwPanelDCLK;            // 51.25 MHz
    45,  // MS_U8 m_dwPanelMinDCLK;         // 45 MHz

    0,  // MS_U16 m_wSpreadSpectrumStep;
    0,  // MS_U16 m_wSpreadSpectrumSpan;
    160, // MS_U8 m_ucDimmingCtl;
    255, // MS_U8 m_ucMaxPWMVal;
    80,  // MS_U8 m_ucMinPWMVal;

    0,   // MS_U8 m_bPanelDeinterMode   :1;
    E_MI_PNL_ASPECT_RATIO_WIDE, // MHAL_DISP_PnlAspectRatio_e m_ucPanelAspectRatio;
    0,   // MS_U16 m_u16LVDSTxSwapValue;
    E_MI_PNL_TI_8BIT_MODE, // MHAL_DISP_ApiPnlTiBitMode_e m_ucTiBitMode;
    E_MI_PNL_OUTPUT_8BIT_MODE, // MHAL_DISP_ApiPnlOutPutFormatBitMode_e m_ucOutputFormatBitMode;

    0,   // MS_U8 m_bPanelSwapOdd_RG    :1;
    0,   // MS_U8 m_bPanelSwapEven_RG   :1;
    0,   // MS_U8 m_bPanelSwapOdd_GB    :1;
    0,   // MS_U8 m_bPanelSwapEven_GB   :1;

    0,   // MS_U8 m_bPanelDoubleClk     :1;
    0x001c848e, // MS_U32 m_dwPanelMaxSET;
    0x0018eb59, // MS_U32 m_dwPanelMinSET;
    E_MI_PNL_CHG_HTOTAL,
    0,   // MS_U8 m_bPanelNoiseDith     :1;
    (MI_PANEL_ChannelSwapType_e)0,
    (MI_PANEL_ChannelSwapType_e)1,
    (MI_PANEL_ChannelSwapType_e)2,
    (MI_PANEL_ChannelSwapType_e)3,
    (MI_PANEL_ChannelSwapType_e)4,
};