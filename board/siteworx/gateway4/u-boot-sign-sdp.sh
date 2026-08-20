#!/bin/bash
set -e

# --- Configuration ---
INPUT_FILE="u-boot-dtb.imx"
OUTPUT_FILE="u-boot-signed-sdp.imx"
LOAD_ADDR=$((0x877FF400))
ALIGN_SIZE=$((0x1000))    # 4 KiB Page Alignment
SIG_RESERVE=$((0x2000))   # 8 KiB reserved for the CST signature

# --- 1. Dynamic Size Calculations ---
FILE_SIZE=$(stat -c%s "$INPUT_FILE")

# Round up to the next 4 KiB boundary that fits the file + signature
TARGET_SIZE=$(( (FILE_SIZE + SIG_RESERVE + ALIGN_SIZE - 1) / ALIGN_SIZE * ALIGN_SIZE ))
PRE_PAD_SIZE=$(( TARGET_SIZE - SIG_RESERVE ))

# Calculate CSF Pointer (Load Address + Pre-Pad Size)
CSF_PTR=$(( LOAD_ADDR + PRE_PAD_SIZE ))

# Format as Hex Strings
PRE_PAD_HEX=$(printf "0x%X" $PRE_PAD_SIZE)
TARGET_SIZE_HEX=$(printf "0x%X" $TARGET_SIZE)

# --- 2. Dynamic DCD Extraction for SDP ---
# Extract DCD Pointer (Little-Endian, IVT offset 12)
read dcd0 dcd1 dcd2 dcd3 <<< $(od -An -t x1 -j 12 -N 4 "$INPUT_FILE")
DCD_PTR=$(( 0x${dcd3}${dcd2}${dcd1}${dcd0} ))

# Extract Self Pointer (Load Address) (Little-Endian, IVT offset 20)
read slf0 slf1 slf2 slf3 <<< $(od -An -t x1 -j 20 -N 4 "$INPUT_FILE")
SELF_PTR=$(( 0x${slf3}${slf2}${slf1}${slf0} ))

# Calculate DCD File Offset
DCD_OFFSET=$(( DCD_PTR - SELF_PTR ))
DCD_OFFSET_HEX=$(printf "0x%03x" $DCD_OFFSET)

# Extract DCD Length (Big-Endian, bytes 1 and 2 of the DCD header)
read len0 len1 <<< $(od -An -t x1 -j $((DCD_OFFSET + 1)) -N 2 "$INPUT_FILE")
DCD_LEN=$(( 0x${len0}${len1} ))
DCD_LEN_HEX=$(printf "0x%03x" $DCD_LEN)

echo "Input Size:  $FILE_SIZE bytes"
echo "Pre-Pad To:  $PRE_PAD_HEX"
echo "Final Size:  $TARGET_SIZE_HEX (Aligned to 4 KiB)"
echo "DCD Offset:  $DCD_OFFSET_HEX"
echo "DCD Length:  $DCD_LEN_HEX"

# --- 3. Convert to Little-Endian for Injection ---
CSF_HEX=$(printf "%08X" $CSF_PTR)
BD_HEX=$(printf "%08X" $TARGET_SIZE)

# Slice into bytes: B0 B1 B2 B3
C_B0=${CSF_HEX:6:2}; C_B1=${CSF_HEX:4:2}; C_B2=${CSF_HEX:2:2}; C_B3=${CSF_HEX:0:2}
B_B0=${BD_HEX:6:2};  B_B1=${BD_HEX:4:2};  B_B2=${BD_HEX:2:2};  B_B3=${BD_HEX:0:2}

# --- 4. Patching the IVT and Boot Data ---
cp "$INPUT_FILE" u-boot-pad.imx

# Inject CSF Pointer at offset 0x18 (24)
printf "\x$C_B0\x$C_B1\x$C_B2\x$C_B3" | dd of=u-boot-pad.imx bs=1 seek=24 conv=notrunc status=none
# Inject Boot Data Length at offset 0x24 (36)
printf "\x$B_B0\x$B_B1\x$B_B2\x$B_B3" | dd of=u-boot-pad.imx bs=1 seek=36 conv=notrunc status=none

# --- 5. Pre-Signature Gap Fill (0xFF) ---
objcopy -I binary -O binary --pad-to=$PRE_PAD_HEX --gap-fill=0xff u-boot-pad.imx u-boot-pad.imx

# --- 6. Generate CSF File ---
cat << EOF > csf_uboot_auto-sdp.txt
[Header]
  Version = 4.0
  Hash Algorithm = sha256
  Engine = SW
  Certificate Format = X509
  Signature Format = CMS

[Install SRK]
  File = "crts/SRK_1_2_3_4_table.bin"
  Source index = 0

[Install CSFK]
  File = "crts/CSF1_1_sha256_4096_65537_v3_usr_crt.pem"

[Authenticate CSF]

[Install Key]
  Verification index = 0
  Target index = 2
  File = "crts/IMG1_1_sha256_4096_65537_v3_usr_crt.pem"

[Authenticate Data]
  Verification index = 2
  Engine = SW
  Blocks = 0x00910000 $DCD_OFFSET_HEX $DCD_LEN_HEX "u-boot-pad.imx", \\
           0x877ff400 0x000 $PRE_PAD_HEX "u-boot-pad.imx"
EOF

# --- 7. Sign and Assemble ---
./linux64/bin/cst -i csf_uboot_auto-sdp.txt -o csf_uboot-sdp.bin
cat u-boot-pad.imx csf_uboot-sdp.bin > u-boot-signed-tmp-sdp.imx

# --- 8. Final Alignment Gap Fill (0xFF) ---
objcopy -I binary -O binary --pad-to=$TARGET_SIZE_HEX --gap-fill=0xff u-boot-signed-tmp-sdp.imx "$OUTPUT_FILE"

# Cleanup
rm u-boot-pad.imx csf_uboot-sdp.bin u-boot-signed-tmp-sdp.imx csf_uboot_auto-sdp.txt
echo "Success: $OUTPUT_FILE generated!"