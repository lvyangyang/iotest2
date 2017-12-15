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


#define MYWDF_KDEVICE L"\\Device\\MyWDF_Device"//�豸���ƣ������ں�ģʽ�µ���������ʹ��  
#define MYWDF_LINKNAME L"\\DosDevices\\MyWDF_LINK"//�������ӣ������û�ģʽ�µĳ������ʹ����������豸��  
#define IOCTL_TRY \
    CTL_CODE( FILEIO_TYPE, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS  )



//�����ص�  
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

	//�����������  
	WDF_DRIVER_CONFIG cfg;//����������  
	WDF_IO_QUEUE_CONFIG      ioQueueConfig;
	WDFQUEUE                            queue;
	WDFDRIVER drv = NULL;//wdf framework ��������  

						 //�豸�������  
	PWDFDEVICE_INIT device_init = NULL;
	UNICODE_STRING ustring;
	WDFDEVICE control_device;
	PDEVICE_OBJECT dev = NULL;

	KdPrint(("DriverEntry �[[start]\n"));


	//��ʼ��WDF_DRIVER_CONFIG  
	WDF_DRIVER_CONFIG_INIT(
		&cfg,
		NULL //���ṩAddDevice����  
	);

	cfg.DriverInitFlags = WdfDriverInitNonPnpDriver;  //ָ����pnp����  
	cfg.EvtDriverUnload = EvtDriverUnload;  //ָ��ж�غ���  

											//����һ��framework����������WDF�������棬WdfDriverCreate�Ǳ���Ҫ���õġ�  
											//framework������������������wdf����ĸ����󣬻��仰˵framework����������wdf�������Ķ��㣬��û�и������ˡ�  
	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &cfg, &drv);
	if (!NT_SUCCESS(status))
	{
		goto DriverEntry_Complete;
	}

	KdPrint(("Create wdf driver object successfully\n"));


	//����һ���豸  

	//��Ҫ����һ���ڴ�WDFDEVICE_INIT,����ڴ��ڴ����豸��ʱ����õ���  
	device_init = WdfControlDeviceInitAllocate(drv, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R);
	if (device_init == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto DriverEntry_Complete;
	}

	//�����豸�����֣��ں�ģʽ�£���������: L"\\Device\\MyWDF_Device"  
	RtlInitUnicodeString(&ustring, MYWDF_KDEVICE);

	//���豸���ִ���device_init��  
	status = WdfDeviceInitAssignName(device_init, &ustring);

	if (!NT_SUCCESS(status))
	{
		goto DriverEntry_Complete;
	}

	KdPrint(("Device name Unicode string: %wZ (this name can only be used by other kernel mode code, like other drivers)\n", &ustring));




	//��ʼ���豸����  

	WDF_OBJECT_ATTRIBUTES_INIT(&object_attribs);

	//����ǰ�洴����device_init������һ���豸. (control device)  
	status = WdfDeviceCreate(&device_init, &object_attribs, &control_device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("create device failed\n"));
		goto DriverEntry_Complete;
	}

	//�����������ӣ������û�ģʽ�µĳ������ʹ���������������Ǳ���ģ���Ȼ�û�ģʽ�µĳ����ܷ�������豸��  
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

	WdfControlFinishInitializing(control_device);//�����豸��ɡ�  




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
	//��ʼ����������dpc
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
	//	׼����������CPU
		if (init_irql < DISPATCH_LEVEL)
		KeRaiseIrql(DISPATCH_LEVEL,&init_irql);
		current_processor = KeGetCurrentProcessorNumber();
		for (size_t i = 0; i < cpu_count; i++)
		{
			if (i != current_processor)
			{	//����������
				KeAcquireSpinLockAtDpcLevel(&notify_lock[i]);
				KeAcquireSpinLockAtDpcLevel(&cpu_lock[i]);
				//����dpc
				KeSetImportanceDpc(&lock_dpc[i], MediumImportance);
				KeSetTargetProcessorDpc(&lock_dpc[i],i);
				KeInsertQueueDpc(&lock_dpc[i],(PVOID)&notify_lock[i],(PVOID)&cpu_lock[i]);
			}
			

		}
		//�ȴ�����cpu����
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

	KeReleaseSpinLock(notify_lock,DISPATCH_LEVEL);//֪ͨ �Ѿ�����CPU
	KeAcquireSpinLockAtDpcLevel(cpu_lock);//�ȴ��������
//	KeReleaseSpinLock(cpu_lock, new_irql);
	
}