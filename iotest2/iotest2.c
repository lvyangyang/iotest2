#include <fltKernel.h>  
#include <wdf.h>  
#include <stdlib.h>
#include <Ntddk.h>
#include <wdfdriver.h>  
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <wdfrequest.h> 
#include <wdm.h>

#include "mem.h"

#define PAGEDCODE code_seg("PAGE");  
#define LOCKEDCODE code_seg();  
#define INITCODE code_seg("INIT");  

#define PAGEDDATA code_seg("PAGE");  
#define LOCKEDDATA code_seg();  
#define INITDATA code_seg("INIT");
#define MAX_CPUCOUNT 32
#define FILEIO_TYPE 40001


#define MYWDF_KDEVICE L"\\Device\\MyWDF_Device"//设备名称，其他内核模式下的驱动可以使用  
#define MYWDF_LINKNAME L"\\DosDevices\\MyWDF_LINK"//符号连接，这样用户模式下的程序可以使用这个驱动设备。  
#define IOCTL_TRY \
    CTL_CODE( FILEIO_TYPE, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS  )



//声明回调  
EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL  IoControlEvtIoDeviceControl;

VOID cpu_lock_function(
	IN struct _KDPC  *Dpc,
	IN PVOID  DeferredContext,
	IN PVOID  SystemArgument1,
	IN PVOID  SystemArgument2
);

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES object_attribs;

	//驱动对象相关  
	WDF_DRIVER_CONFIG cfg;//驱动的配置  
	WDF_IO_QUEUE_CONFIG      ioQueueConfig;
	WDFQUEUE                            queue;
	WDFDRIVER drv = NULL;//wdf framework 驱动对象  

						 //设备对象相关  
	PWDFDEVICE_INIT device_init = NULL;
	UNICODE_STRING ustring;
	WDFDEVICE control_device;
	PDEVICE_OBJECT dev = NULL;

	KdPrint(("DriverEntry [start]\n"));


	//初始化WDF_DRIVER_CONFIG  
	WDF_DRIVER_CONFIG_INIT(
		&cfg,
		NULL //不提供AddDevice函数  
	);

	cfg.DriverInitFlags = WdfDriverInitNonPnpDriver;  //指定非pnp驱动  
	cfg.EvtDriverUnload = EvtDriverUnload;  //指定卸载函数  

											//创建一个framework驱动对象，在WDF程序里面，WdfDriverCreate是必须要调用的。  
											//framework驱动对象是其他所有wdf对象的父对象，换句话说framework驱动对象是wdf对象树的顶点，它没有父对象了。  
	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &cfg, &drv);
	if (!NT_SUCCESS(status))
	{
		goto DriverEntry_Complete;
	}

	KdPrint(("Create wdf driver object successfully\n"));


	//创建一个设备  

	//先要分配一块内存WDFDEVICE_INIT,这块内存在创建设备的时候会用到。  
	device_init = WdfControlDeviceInitAllocate(drv, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R);
	if (device_init == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto DriverEntry_Complete;
	}

	//创建设备的名字，内核模式下，名字类似: L"\\Device\\MyWDF_Device"  
	RtlInitUnicodeString(&ustring, MYWDF_KDEVICE);

	//将设备名字存入device_init中  
	status = WdfDeviceInitAssignName(device_init, &ustring);

	if (!NT_SUCCESS(status))
	{
		goto DriverEntry_Complete;
	}

	KdPrint(("Device name Unicode string: %wZ (this name can only be used by other kernel mode code, like other drivers)\n", &ustring));




	//初始化设备属性  

	WDF_OBJECT_ATTRIBUTES_INIT(&object_attribs);

	//根据前面创建的device_init来创建一个设备. (control device)  
	status = WdfDeviceCreate(&device_init, &object_attribs, &control_device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("create device failed\n"));
		goto DriverEntry_Complete;
	}

	//创建符号连接，这样用户模式下的程序可以使用这个驱动。这个是必须的，不然用户模式下的程序不能访问这个设备。  
	RtlInitUnicodeString(&ustring, MYWDF_LINKNAME);
	status = WdfDeviceCreateSymbolicLink(control_device, &ustring);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create Link\n"));
		goto DriverEntry_Complete;
	}

	KdPrint(("Create symbolic link successfully, %wZ (user mode code should use this name, like in CreateFile())\n", &ustring));

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
		WdfIoQueueDispatchSequential);
	ioQueueConfig.EvtIoDeviceControl = IoControlEvtIoDeviceControl;
	__analysis_assume(ioQueueConfig.EvtIoStop != 0);
	status = WdfIoQueueCreate(control_device,
		&ioQueueConfig,
		&object_attribs,
		&queue // pointer to default queue
	);
	__analysis_assume(ioQueueConfig.EvtIoStop == 0);

	WdfControlFinishInitializing(control_device);//创建设备完成。  




	KdPrint(("Create device object successfully\n"));


	KdPrint(("DriverEntry succeeds [end]\n"));

DriverEntry_Complete:

	return status;
}


static VOID EvtDriverUnload(WDFDRIVER Driver)
{
	KdPrint(("unload driver\n"));
	KdPrint(("Doesn't need to clean up the devices, since we only have control device here\n"));
}/* EvtDriverUnload */

#pragma LOCKEDCODE
VOID IoControlEvtIoDeviceControl(
	IN WDFQUEUE         Queue,
	IN WDFREQUEST       Request,
	IN size_t            OutputBufferLength,
	IN size_t            InputBufferLength,
	IN ULONG            IoControlCode

) {
	NTSTATUS  status = STATUS_SUCCESS;
	PVOID               inBuf = NULL, outBuf = NULL;
	size_t               bufSize;
	double data;
	KIRQL init_irql=KeGetCurrentIrql();
	KIRQL temp_irql;
	//size_t size = sizeof(double);
	state_t mem_state;
	iter_t  iter = 1;
//	size_t i=0;
	ULONG cpu_count = KeQueryActiveProcessorCount(NULL);
	ULONG current_processor ;
	LARGE_INTEGER m_start;
	LARGE_INTEGER m_stop;
	LARGE_INTEGER m_frequency;
	KeQueryPerformanceCounter(&m_frequency);
	KSPIN_LOCK cpu_lock[MAX_CPUCOUNT];
	KSPIN_LOCK notify_lock[MAX_CPUCOUNT];
	KDPC lock_dpc[MAX_CPUCOUNT];
	//初始化自旋锁和dpc
	for (size_t i = 0; i < cpu_count; i++)
	{	
		KeInitializeSpinLock(&cpu_lock[i]);
		KeInitializeSpinLock(&notify_lock[i]);
		KeInitializeDpc(&lock_dpc[i],(PKDEFERRED_ROUTINE)cpu_lock_function,NULL);

	}
	
	if (!OutputBufferLength && !InputBufferLength)
	{
		WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		return;
	}


	switch (IoControlCode)
	{
	case IOCTL_TRY:
		
		
		status = WdfRequestRetrieveOutputBuffer(Request, 0, &outBuf, &bufSize);
	
		mem_state.nbytes = 1024 * 1024 * 200;
		mem_state.need_buf2 = 0;
		init_loop(0, &mem_state);
		double duration = 0;
	//	准备锁定其他CPU
		if (init_irql < DISPATCH_LEVEL)
		KeRaiseIrql(DISPATCH_LEVEL,&init_irql);
		current_processor = KeGetCurrentProcessorNumber();
		for (size_t i = 0; i < cpu_count; i++)
		{
			if (i != current_processor)
			{	//设置自旋锁
				KeAcquireSpinLockAtDpcLevel(&notify_lock[i]);
				KeAcquireSpinLockAtDpcLevel(&cpu_lock[i]);
				//设置dpc
				KeSetImportanceDpc(&lock_dpc[i], MediumImportance);
				KeSetTargetProcessorDpc(&lock_dpc[i],i);
				KeInsertQueueDpc(&lock_dpc[i],(PVOID)&notify_lock[i],(PVOID)&cpu_lock[i]);
			}
			

		}
		//等待其他cpu锁定
		for (size_t i = 0; i < cpu_count; i++)
		{
			if (i != current_processor)
			KeAcquireSpinLock(&notify_lock[i], &temp_irql);
						
		}
	
		while (TRUE)
		{
			m_start = KeQueryPerformanceCounter(&m_frequency);

			wr(iter, &mem_state);

			m_stop = KeQueryPerformanceCounter(NULL);
			duration = (double)(m_stop.QuadPart-m_start.QuadPart)/(double)m_frequency.QuadPart;
			iter = iter * 2;
			if (duration > 0.155)
				break;
		}
	
		for (size_t i = 0; i < cpu_count; i++)
		{
			if (i != current_processor)
				KeReleaseSpinLock(&cpu_lock[i], DISPATCH_LEVEL);

		}
		KeLowerIrql(init_irql);
		cleanup(iter, &mem_state);
		data = (double)iter*mem_state.nbytes / (1024 * 1024 * 1024) / duration;
		RtlCopyMemory(outBuf, &data, sizeof(double));
		WdfRequestSetInformation(Request, bufSize);
		break;
	default:
		break;
	}

	WdfRequestComplete(Request, status);


}

VOID cpu_lock_function(
	IN struct _KDPC  *Dpc,
	IN PVOID  DeferredContext,
	IN PVOID  SystemArgument1,
	IN PVOID  SystemArgument2
)
{
	
	PKSPIN_LOCK notify_lock = (PKSPIN_LOCK)SystemArgument1;
	PKSPIN_LOCK cpu_lock = (PKSPIN_LOCK)SystemArgument2;

	KeReleaseSpinLock(notify_lock,DISPATCH_LEVEL);//通知 已经锁定CPU
	KeAcquireSpinLockAtDpcLevel(cpu_lock);//等待测试完成
//	KeReleaseSpinLock(cpu_lock, new_irql);
	
}