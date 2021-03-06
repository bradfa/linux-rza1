#
# Makefile for the linux kernel.
#

# Common objects
obj-y				:= timer.o console.o clock.o

# CPU objects
obj-$(CONFIG_ARCH_SH7372)	+= setup-sh7372.o clock-sh7372.o intc-sh7372.o
obj-$(CONFIG_ARCH_SH73A0)	+= setup-sh73a0.o clock-sh73a0.o intc-sh73a0.o
obj-$(CONFIG_ARCH_RZA1)		+= setup-rza1.o clock-rza1.o intc-rza1.o pfc-rza1.o
obj-$(CONFIG_ARCH_R8A7740)	+= setup-r8a7740.o clock-r8a7740.o intc-r8a7740.o
obj-$(CONFIG_ARCH_R8A7779)	+= setup-r8a7779.o clock-r8a7779.o intc-r8a7779.o
obj-$(CONFIG_ARCH_EMEV2)	+= setup-emev2.o clock-emev2.o

# SMP objects
smp-y				:= platsmp.o headsmp.o
smp-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o
smp-$(CONFIG_ARCH_SH73A0)	+= smp-sh73a0.o
smp-$(CONFIG_ARCH_R8A7779)	+= smp-r8a7779.o
smp-$(CONFIG_ARCH_EMEV2)	+= smp-emev2.o

# Pinmux setup
pfc-y				:=
pfc-$(CONFIG_ARCH_SH7372)	+= pfc-sh7372.o
pfc-$(CONFIG_ARCH_SH73A0)	+= pfc-sh73a0.o
pfc-$(CONFIG_ARCH_R8A7740)	+= pfc-r8a7740.o
pfc-$(CONFIG_ARCH_R8A7779)	+= pfc-r8a7779.o

# IRQ objects
obj-$(CONFIG_ARCH_SH7372)	+= entry-intc.o
obj-$(CONFIG_ARCH_R8A7740)	+= entry-intc.o

# PM objects
obj-$(CONFIG_SUSPEND)		+= suspend.o
obj-$(CONFIG_CPU_IDLE)		+= cpuidle.o
obj-$(CONFIG_ARCH_SHMOBILE)	+= pm-rmobile.o
obj-$(CONFIG_ARCH_SH7372)	+= pm-sh7372.o sleep-sh7372.o
obj-$(CONFIG_ARCH_R8A7740)	+= pm-r8a7740.o
obj-$(CONFIG_ARCH_R8A7779)	+= pm-r8a7779.o

# Board objects
obj-$(CONFIG_MACH_AP4EVB)	+= board-ap4evb.o
obj-$(CONFIG_MACH_AG5EVM)	+= board-ag5evm.o
obj-$(CONFIG_MACH_MACKEREL)	+= board-mackerel.o
obj-$(CONFIG_MACH_KOTA2)	+= board-kota2.o
obj-$(CONFIG_MACH_BONITO)	+= board-bonito.o
obj-$(CONFIG_MACH_MARZEN)	+= board-marzen.o
obj-$(CONFIG_MACH_ARMADILLO800EVA)	+= board-armadillo800eva.o
obj-$(CONFIG_MACH_KZM9D)	+= board-kzm9d.o
obj-$(CONFIG_MACH_KZM9G)	+= board-kzm9g.o
obj-$(CONFIG_MACH_RSKRZA1)	+= board-rskrza1.o

# Framework support
obj-$(CONFIG_SMP)		+= $(smp-y)
obj-$(CONFIG_GENERIC_GPIO)	+= $(pfc-y)
