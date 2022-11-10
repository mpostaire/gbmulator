package io.github.mpostaire.gbmulator.drive;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.api.client.http.FileContent;
import com.google.api.services.drive.Drive;
import com.google.api.services.drive.model.File;
import com.google.api.services.drive.model.FileList;

import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class DriveServiceHelper {

    private final Executor executor = Executors.newSingleThreadExecutor();

    private Drive driveService;

    public DriveServiceHelper(Drive driveService) {
        this.driveService = driveService;
    }

    public Task<String> createSaveDir() {
        return Tasks.call(executor, () -> {
            File dirMetadata = new File();
            dirMetadata.setName("GBmulator_saves");
            dirMetadata.setMimeType("application/vnd.google-apps.folder");
            File dir = driveService.files().create(dirMetadata)
                    .setFields("id")
                    .execute();
            if (dir == null)
                throw new IOException("Null result when requesting save dir creation");
            return dir.getId();
        });
    }

    public Task<ArrayList<java.io.File>> uploadSaveFiles(ArrayList<java.io.File> toUpload, String parentDirId) {
        return Tasks.call(executor, () -> {
            ArrayList<java.io.File> uploadedFiles = new ArrayList<>();
            for (java.io.File src : toUpload) {
                java.io.File file = new java.io.File(src.getAbsolutePath());
                FileContent mediaContent = new FileContent("application/octet-stream", file);
                File fileMetadata = new File();
                fileMetadata.setName(file.getName());
                fileMetadata.setParents(Collections.singletonList(parentDirId));
                File myFile = driveService.files().create(fileMetadata, mediaContent).execute();
                if (myFile == null) continue;
                uploadedFiles.add(src);
            }

            return uploadedFiles;
        });
    }

    public Task<ArrayList<File>> downloadSaveFiles(ArrayList<File> toDownload, String destDirPath) {
        return Tasks.call(executor, () -> {
            ArrayList<File> downloadedFiles = new ArrayList<>();
            for (File src : toDownload) {
                ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
                driveService.files().get(src.getId()).executeMediaAndDownloadTo(byteArrayOutputStream);
                try {
                    FileOutputStream fileOutputStream = new FileOutputStream(destDirPath + "/" + src.getName());
                    byteArrayOutputStream.writeTo(fileOutputStream);
                    downloadedFiles.add(src);
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
            return downloadedFiles;
        });
    }

    public Task<ArrayList<File>> updateSaveFiles(ArrayList<String> toUpdateLocalPath, ArrayList<File> toUpdate) {
        return Tasks.call(executor, () -> {
            ArrayList<File> updatedFiles = new ArrayList<>();
            for (int i = 0; i < toUpdate.size(); i++) {
                java.io.File fileContents = new java.io.File(toUpdateLocalPath.get(i));
                FileContent mediaContent = new FileContent("application/octet-stream", fileContents);

                File newDistantFile = new File();

                File myFile = driveService.files().update(toUpdate.get(i).getId(), newDistantFile, mediaContent).execute();
                if (myFile == null) continue;
                updatedFiles.add(toUpdate.get(i));
            }
            return updatedFiles;
        });
    }

    public Task<String> getSaveDirId() {
        return Tasks.call(executor, () -> {
            FileList fileList = driveService.files().list()
                    .setQ("mimeType = 'application/vnd.google-apps.folder' and trashed = false and name = 'GBmulator_saves'")
                    .setSpaces("drive")
                    .setFields("nextPageToken, files(id, name)")
                    .execute();

            if (fileList.isEmpty()) {
                return null;
            }
            return fileList.getFiles().get(0).getId();
        });
    }

    public Task<List<File>> listSaveFiles(String dirId) {
        return Tasks.call(executor, () -> {
            FileList fileList = driveService.files().list()
                    .setQ("'" + dirId + "' in parents and mimeType != 'application/vnd.google-apps.folder' and trashed = false")
                    .setSpaces("drive")
                    .setFields("nextPageToken, files(id, name, parents, modifiedTime)")
                    .execute();

            return fileList.getFiles();
        });
    }
}
