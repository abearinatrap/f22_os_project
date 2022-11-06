package edu.cs300;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;
import java.util.Scanner;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.concurrent.ArrayBlockingQueue;

class Worker extends Thread{
	  TreeMap<LocalDateTime, Meeting> calendar;
	  ArrayBlockingQueue<MeetingRequest> incomingRequests;
	  ArrayBlockingQueue<MeetingResponse> outgoingResponse;
	  String empId;
	  String empFilename;

	  public Worker(String empId, String empFilename, ArrayBlockingQueue<MeetingRequest> incomingRequests, ArrayBlockingQueue<MeetingResponse> outgoingResponse){
	    this.incomingRequests=incomingRequests;
	    this.outgoingResponse=outgoingResponse;
		this.empId=empId;
		this.empFilename=empFilename;
		this.calendar = new TreeMap<LocalDateTime, Meeting>();
		  File empFile = new File(empFilename);
		  if(!empFile.exists()){
			  DebugLog.log("Employees file not found. Please run in correct directory");
		  }
		  try (Scanner scanner = new Scanner(empFile)) {
			  while (scanner.hasNextLine()) {
				  List<String> values = new ArrayList<String>();
				  try (Scanner rowScanner = new Scanner(scanner.nextLine())) {
					  rowScanner.useDelimiter(",");
					  while (rowScanner.hasNext()) {
						  String value=rowScanner.next();
						  values.add(value);
					  }
				  }
				  if(values.size()==0) {
					  break;
				  }
				  Meeting meeting = new Meeting(values.get(0),values.get(1),values.get(2),Integer.parseInt(values.get(3)));
				  //DebugLog.log(LocalDateTime.parse(meeting.datetime).toString());
				  calendar.put(LocalDateTime.parse(meeting.datetime), meeting);
			  }
		  } catch (FileNotFoundException e) {
			  //handle
		  }
	  }

	  public void run() {
	    DebugLog.log(" Thread ("+this.empId+") thread started ...");
		try {
		while(true) {
			MeetingRequest mtgReq = (MeetingRequest) this.incomingRequests.take();
			Boolean accept=true;
			if(mtgReq.request_id==0){
				//exit, first writing data
				try(FileWriter fw = new FileWriter(new File(this.empFilename+".bak"))){
					for( Meeting m : this.calendar.values()){
						fw.write(m.toString()+"\n");
					}
				} catch ( IOException e) {

				}
				return;
			}
			if(calendar.containsKey(LocalDateTime.parse(mtgReq.datetime))){
				DebugLog.log("exact time match\n");
				this.outgoingResponse.put(new MeetingResponse(mtgReq.request_id, 0));
				continue;
			}else{
				LocalDateTime original = LocalDateTime.parse(mtgReq.datetime);
				calendar.put(original, new Meeting(mtgReq.description, mtgReq.location, mtgReq.datetime, mtgReq.duration));
				//ChronoUnit
				LocalDateTime lower = calendar.lowerKey(original);
				if(lower!=null){
					if(!original.isAfter(lower.plusMinutes(calendar.get(lower).duration))){
						accept=false;
						DebugLog.log("meeting before\n");
					}
				}
				LocalDateTime higher = calendar.higherKey(original);
				if(higher!=null){
					if(!higher.isAfter(original.plusMinutes(mtgReq.duration))){
						accept=false;
						DebugLog.log("meeting after\n");
					}
				}
			}
			DebugLog.log("Worker-" + this.empId + " " + mtgReq + " pushing response " + mtgReq.request_id + " spots left" + this.outgoingResponse.remainingCapacity());
			if(accept){
				this.outgoingResponse.put(new MeetingResponse(mtgReq.request_id, 1));
			}else{
				calendar.remove(LocalDateTime.parse(mtgReq.datetime));
				this.outgoingResponse.put(new MeetingResponse(mtgReq.request_id, 0));
			}
		}
		} catch(InterruptedException e){
			System.err.println(e.getMessage());
		}
	    
	  }

	}
