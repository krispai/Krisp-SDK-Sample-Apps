//
//  WelcomeView.swift
//  KrispTestApp
//
//  Created by Aram Tatalyan on 13.01.25.
//

import SwiftUI

struct WelcomeView: View {
    @ObservedObject var appState: AppState
    
    var body: some View {
        VStack(spacing: 40) {
            Spacer()
            HStack {
                Spacer()
                Image("KrispIcon")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .padding(.top, 40)
                    .frame(minWidth: 100, maxWidth: 115)
                Text("Krisp Test\nApplication")
                    .multilineTextAlignment(TextAlignment.leading)
                    .font(.largeTitle)
                    .fontWeight(.bold)
                    //.foregroundColor(Color.primary)
                    .lineLimit(2, reservesSpace: true)
                    .minimumScaleFactor(0.5)
                    //.layoutPriority(1)
                Spacer()
            }
            Text("Krisp SDK Version: \(appState.versionText)")
                .multilineTextAlignment(TextAlignment.leading)
                .font(.title)
                .fontWeight(.bold)
                .minimumScaleFactor(0.5)
            Text("Select Testing Mode")
                .multilineTextAlignment(TextAlignment.leading)
                .font(.title)
                .fontWeight(.bold)
                .minimumScaleFactor(0.5)
            NavigationLink(destination: FileProcessingView()) {
                Text("File Processing")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.blue)
                    .cornerRadius(8)
                    .foregroundColor(.white)
                    .padding(.horizontal, 20)
            }
            NavigationLink(destination: AudioCaptureView()) {
                Text("Audio Capture")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.green.cornerRadius(8))
                    .foregroundColor(.white)
                    .padding(.horizontal, 20)
            }
            Spacer()
            Spacer()
        }
    }
}

#Preview {
    WelcomeView(appState: AppState())
}
