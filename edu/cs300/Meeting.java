package edu.cs300;
public class Meeting {
    public Meeting(String description, String location, String datetime,
                          int duration) {
        this.description = description;
        this.location = location;
        this.datetime = datetime;
        this.duration = duration;
    }
    String description;
    String location;
    String datetime;//2022-12-20T08:30
    int duration;

    @Override
    public String toString() {
        return this.description + ","+this.location+ ","+this.datetime+ ","+this.duration;
    }

}
