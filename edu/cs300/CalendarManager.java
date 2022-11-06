package edu.cs300;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;
import java.util.Scanner;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;


public class CalendarManager {
	
	Hashtable<String,ArrayBlockingQueue<MeetingRequest>> empQueueMap;
	ArrayBlockingQueue<MeetingResponse> resultsOutputArray;

	InputQueueProcessor iqp;
	OutputQueueProcessor oqp;
	public CalendarManager() {
		this.resultsOutputArray = new ArrayBlockingQueue<MeetingResponse>(30);
		empQueueMap = new Hashtable<String,ArrayBlockingQueue<MeetingRequest>>();
		//read employees.csv and create new
		File employeescsv = new File("employees.csv");
		if(!employeescsv.exists()){
			DebugLog.log("Employees file not found. Please run in correct directory");
		}
		try (Scanner scanner = new Scanner(employeescsv)) {
			while (scanner.hasNextLine()) {
				List<String> values = new ArrayList<String>();
				try (Scanner rowScanner = new Scanner(scanner.nextLine())) {
					rowScanner.useDelimiter(",");
					while (rowScanner.hasNext()) {
						values.add(rowScanner.next());
					}
				}
				if(values.size()==0) {
					break;
				}

				ArrayBlockingQueue<MeetingRequest> newQ = new ArrayBlockingQueue<MeetingRequest>(10);
				empQueueMap.put(values.get(0), newQ);
				new Worker(values.get(0), values.get(1), newQ, this.resultsOutputArray).start();
			}
		} catch (FileNotFoundException e) {
			//handle
		}

		/*
		ArrayBlockingQueue<MeetingRequest> queue1234 = new  ArrayBlockingQueue<MeetingRequest>(10);
		ArrayBlockingQueue<MeetingRequest> queue4567 = new  ArrayBlockingQueue<MeetingRequest>(10);
		empQueueMap.put("1234", queue1234);
		empQueueMap.put("4567", queue4567);
		new Worker("1234",queue1234, this.resultsOutputArray).start();      
		new Worker("4567",queue4567, this.resultsOutputArray).start();      

		*/

		this.iqp = new InputQueueProcessor(this.empQueueMap);
		this.iqp.start();
		this.oqp = new OutputQueueProcessor(this.resultsOutputArray);
		this.oqp.start();

	}

	public static void main(String args[]) {
		
		CalendarManager mgr = new CalendarManager();

		try {
			mgr.iqp.join();
		} catch (InterruptedException e){

		}
		mgr.oqp.interrupt();
	}
	
	class OutputQueueProcessor extends Thread {
		
		ArrayBlockingQueue<MeetingResponse> resultsOutputArray;
		
		OutputQueueProcessor(ArrayBlockingQueue<MeetingResponse> resultsOutputArray){
			this.resultsOutputArray=resultsOutputArray;
		}
		
		public void run() {
			while (true) {
				try {
					MeetingResponse res = resultsOutputArray.take();
	
					MessageJNI.writeMtgReqResponse(res.request_id, res.avail);
					DebugLog.log(getName()+" writing response "+res);
					
				} catch (InterruptedException e) {
					MessageJNI.writeMtgReqResponse(0, 0);
					return;
				} catch (Exception e) {
					DebugLog.log("Sys5OutputQueueProcessor error "+e.getMessage());
				}

			}
			
		}
		
	}

	class InputQueueProcessor extends Thread {
		Hashtable<String,ArrayBlockingQueue<MeetingRequest>> empQueueMap;
		
		InputQueueProcessor(Hashtable<String,ArrayBlockingQueue<MeetingRequest>> empQueueMap){
			this.empQueueMap=empQueueMap;
		}

		public void run(){
			while (true) {
				MeetingRequest req = MessageJNI.readMeetingRequest();
				try {
					if(req==null){
						continue;
					}
					DebugLog.log(getName()+"recvd msg from queue for "+req.empId);
					if(req.request_id==0){
						for( ArrayBlockingQueue<MeetingRequest> m: empQueueMap.values()){
							m.put(req);
						}
						//wait until all workers quit
						return;
					}
					if (empQueueMap.containsKey(req.empId)) {
						empQueueMap.get(req.empId).put(req);
						DebugLog.log(getName()+" pushing req "+req+" to "+req.empId);
					}
					
				} catch (InterruptedException e) {
					DebugLog.log(getName()+" Error putting to emp queue"+req.empId);					
					e.printStackTrace();
				}
			}
		}
		
	}

}


