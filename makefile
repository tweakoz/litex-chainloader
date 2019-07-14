include ${SOC_BUILD_DIR}/${FPGAPLAT}/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

###############################################################################

OUTDIR = ${SOC_BUILD_DIR}/${FPGAPLAT}/software/chainloader

LIBS = -lcompiler_rt -lbase-nofloat

LIBDIRS = -L$(BUILDINC_DIRECTORY)/../libnet \
          -L$(BUILDINC_DIRECTORY)/../libbase \
          -L$(BUILDINC_DIRECTORY)/../libcompiler_rt

INCDIRS = -I$(BUILDINC_DIRECTORY) \
          -I$(SOC_DIRECTORY)/software/include \
          -I$(SOC_DIRECTORY)/software/include/base \

DEFS = -D__vexriscv__ -D__LOCALIP__=${SOC_TFTP_CLIENT_IP} -D__REMOTEIP__=${SOC_TFTP_SERVER_IP} -D__TFTPPORT__=${SOC_TFTP_SERVER_PORT}
CCFLAGS = $(DEFS) -march=rv32im -mabi=ilp32 -N -Os -Wno-builtin-declaration-mismatch
CXXFLAGS = $(CCFLAGS) -std=c++17 -fno-exceptions

###############################################################################

${OUTDIR}/chainloader.bin: ${OUTDIR}/chainloader.elf
	riscv64-unknown-elf-objcopy -O binary $< $@

###############################################################################

CXX_SRCS_1 := main.cpp netboot.cpp tftp.cpp udp.cpp
CXX_OBJS_1 := $(patsubst %.cpp, ${OUTDIR}/%.o, $(CXX_SRCS_1))

${OUTDIR}/%.o : %.cpp makefile
	riscv64-unknown-elf-g++ $(INCDIRS) $(CXXFLAGS) -c -o $@ $<

###############################################################################

${OUTDIR}/boot-helper.o: $(SOC_DIRECTORY)/software/bios/boot-helper-vexriscv.S makefile
	riscv64-unknown-elf-gcc \
		$(INCDIRS) $(CCFLAGS) -c -o $@ $(SOC_DIRECTORY)/software/bios/boot-helper-vexriscv.S

${OUTDIR}/chainloader.elf: ${CXX_OBJS_1} ${OUTDIR}/boot-helper.o makefile chainloader.ld
	riscv64-unknown-elf-g++ \
		$(LIBDIRS) $(CXXFLAGS) $(LDFLAGS) \
		-T chainloader.ld \
		$(BUILDINC_DIRECTORY)/../libbase/crt0-$(CPU)-ctr.o \
		-Wl,-Map=${OUTDIR}/chainloader.map \
		-Wl,-strip-all \
		-o $@ ${CXX_OBJS_1} ${OUTDIR}/boot-helper.o $(LIBS)

###############################################################################

.PHONY: all

all: ${OUTDIR}/chainloader.bin

$(shell mkdir -p $(OUTDIR))

###############################################################################

clean:
	rm ${OUTDIR}/*.o
	rm ${OUTDIR}/*.elf
	rm ${OUTDIR}/*.bin
